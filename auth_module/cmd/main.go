package main

import (
	"auth/internal/config"
	"auth/internal/database/repository"
	"auth/internal/database/storage"
	"auth/internal/handlers"
	"auth/internal/pkg/jwt"
	"auth/internal/pkg/oauth"
	"auth/internal/services"
	"log"
	"net/http"

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
	defer mongoRepo.Close()

	// Инициализация in-memory хранилища
	memoryStore := storage.NewMemoryStore()

	// Инициализация сервисов
	jwtService := jwt.NewJWTService(cfg)
	oauthService := oauth.NewOAuthService(cfg)
	codeService := services.NewCodeAuthService(memoryStore, cfg.AuthCodeTTL)
	permissionsSvc := services.NewPermissionsService()
	loginStateMgr := repository.NewLoginStateManager(memoryStore, cfg.LoginTokenTTL)

	// Создаем хендлеры
	authHandler := handlers.NewAuthHandler()

	oauthHandler := handlers.NewOAuthHandler(
		oauthService,
		mongoRepo,
		loginStateMgr,
	)

	//Настройка маршрутизатора Gin
	router := gin.Default()

	// Группа маршрутов аутентификации
	authGroup := router.Group("/auth")
	{
		// Инициализация входа
		authGroup.POST("/login", authHandler.InitiateLoginHandler)

		// OAuth callback маршруты
		authGroup.GET("/github/callback", oauthHandler.GitHubCallbackHandler)
		authGroup.GET("/yandex/callback", oauthHandler.YandexCallbackHandler)

	}

	// Health check
	router.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "healthy"})
	})

	log.Println("Starting server on port 8081")
	log.Fatal(router.Run(":8081"))
}
