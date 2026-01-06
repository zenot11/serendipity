package main

import (
	"auth/internal/handlers"
	"log"
	"net/http"

	"github.com/gin-gonic/gin"
)

func main() {

	//Настройка маршрутизатора Gin
	router := gin.Default()

	// Создаем хендлер
	authHandler := handlers.NewAuthHandler()

	// Группа маршрутов аутентификации
	authGroup := router.Group("/auth")
	{
		// Инициализация входа
		authGroup.POST("/login", authHandler.InitiateLoginHandler)
	}

	// Health check
	router.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "healthy"})
	})

	log.Println("Starting server on port 8081")
	log.Fatal(router.Run(":8081"))
}
