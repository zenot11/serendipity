package handlers

import (
	"auth/internal/database/repository"
	"auth/internal/models"
	"net/http"

	"github.com/gin-gonic/gin"
)

type AuthHandler struct {
	loginStateMgr *repository.LoginStateManager
}

func NewAuthHandler() *AuthHandler {
	// Сервисы будут добавлены позже
	return &AuthHandler{}
}

// InitiateLoginHandler - POST /auth/login
func (h *AuthHandler) InitiateLoginHandler(c *gin.Context) {
	var req models.LoginRequest
	if err := c.ShouldBind(&req); err != nil {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Invalid request format"})
		return
	}
	// Создаем состояние входа
	h.loginStateMgr.CreateLoginState(req.LoginToken)

	// Временная заглушка
	switch req.Type {
	case "github", "yandex", "code":
		c.JSON(http.StatusOK, gin.H{
			"message": "Authentication method accepted",
			"type":    req.Type,
		})

	default:
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Unsupported auth type"})
	}
}
