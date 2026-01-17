package main

import (
	"auth/internal/config"
	"auth/internal/database/repository"
	"auth/internal/database/storage"
	"auth/internal/handlers"
	"auth/internal/pkg/cleanup"
	"auth/internal/pkg/jwt"
	"auth/internal/pkg/oauth"
	"auth/internal/services"
	"context"
	"errors"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/gin-gonic/gin"
)

func main() {
	// Загрузка конфигурации
	cfg := config.Load()

	// Инициализация MongoDB
	mongoRepo, err := storage.NewMongoRepository(cfg.MongoDBURI, "auth_service")
	if err != nil {
		log.Fatalf("Failed to connect to MongoDB: %v", err)
	}
	defer func(mongoRepo *storage.MongoRepository) {
		err := mongoRepo.Close()
		if err != nil {

		}
	}(mongoRepo)

	// Инициализация in-memory хранилища
	memoryStore := storage.NewMemoryStore()

	// Инициализация сервисов
	jwtService := jwt.NewJWTService(cfg)
	oauthService := oauth.NewOAuthService(cfg)
	codeService := services.NewCodeAuthService(memoryStore, cfg.AuthCodeTTL)
	permissionsSvc := services.NewPermissionsService()
	loginStateMgr := repository.NewLoginStateManager(memoryStore, cfg.LoginTokenTTL)

	authHandler := handlers.NewAuthHandler(
		jwtService,
		oauthService,
		codeService,
		mongoRepo,
		loginStateMgr,
		permissionsSvc,
	)

	oauthHandler := handlers.NewOAuthHandler(
		jwtService,
		oauthService,
		mongoRepo,
		loginStateMgr,
		permissionsSvc,
	)

	codeHandler := handlers.NewCodeHandler(
		jwtService,
		codeService,
		mongoRepo,
		loginStateMgr,
		permissionsSvc,
	)

	// Запуск фоновой очистки
	cleanupScheduler := cleanup.NewCleanupScheduler(memoryStore, cfg.CleanupInterval)
	cleanupScheduler.Start()

	//Настройка маршрутизатора Gin
	router := gin.Default()
	router.Use(gin.Logger())
	router.Use(gin.Recovery())
	router.Use(CORSMiddleware())

	// Группа маршрутов аутентификации
	authGroup := router.Group("/auth")
	{
		// Инициализация входа
		authGroup.POST("/login", authHandler.InitiateLoginHandler)

		// Проверка статуса входа
		authGroup.GET("/login/status", authHandler.CheckLoginStatusHandler)

		// Обновление токенов
		authGroup.POST("/refresh", authHandler.RefreshTokenHandler)

		// Выход
		authGroup.POST("/logout", authHandler.LogoutHandler)

		// Валидация токена
		authGroup.POST("/validate", authHandler.ValidateTokenHandler)

		// OAuth callback маршруты
		authGroup.GET("/github/callback", oauthHandler.GitHubCallbackHandler)
		authGroup.GET("/yandex/callback", oauthHandler.YandexCallbackHandler)

		// Кодовая аутентификация
		authGroup.POST("/code/callback", codeHandler.CodeCallbackHandler)
	}

	// Health check
	router.GET("/health", authHandler.HealthCheckHandler)

	// Запуск сервера с graceful shutdown
	srv := &http.Server{
		Addr:    ":" + cfg.ServerPort,
		Handler: router,
	}

	// Запуск сервера в горутине
	go func() {
		log.Printf("Starting auth server on port %s", cfg.ServerPort)
		if err := srv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			log.Fatalf("Failed to start server: %v", err)
		}
	}()

	// Graceful shutdown
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	log.Println("Shutting down server...")

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := srv.Shutdown(ctx); err != nil {
		log.Fatal("Server forced to shutdown:", err)
	}

	log.Println("Server exited properly")
}

// CORSMiddleware настраивает CORS
func CORSMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Writer.Header().Set("Access-Control-Allow-Origin", "*")
		c.Writer.Header().Set("Access-Control-Allow-Credentials", "true")
		c.Writer.Header().Set("Access-Control-Allow-Headers", "Content-Type, Content-Length, Accept-Encoding, X-CSRF-Token, Authorization, accept, origin, Cache-Control, X-Requested-With")
		c.Writer.Header().Set("Access-Control-Allow-Methods", "POST, OPTIONS, GET, PUT, DELETE")

		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}

		c.Next()
	}
}
