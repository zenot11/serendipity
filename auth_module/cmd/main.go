package main

import (
	"github.com/gin-gonic/gin"
	"log"
)

func main() {
	//Настройка маршрутизатора Gin
	router := gin.Default()

	//
	router.GET("/", func(c *gin.Context) {
		c.String(200, "Hello!")
	})

	log.Println("Starting server on port 8081")
	log.Fatal(router.Run(":8081"))
}
