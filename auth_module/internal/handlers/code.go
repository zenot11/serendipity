package handlers

import (
	"auth/internal/database/repository"
	"auth/internal/database/storage"
	"auth/internal/models"
	"auth/internal/pkg/jwt"
	"auth/internal/services"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

type CodeHandler struct {
	jwtService     *jwt.JWTService
	codeService    *services.CodeAuthService
	mongoRepo      *storage.MongoRepository
	loginStateMgr  *repository.LoginStateManager
	permissionsSvc *services.PermissionsService
}

func NewCodeHandler(
	jwtService *jwt.JWTService,
	codeService *services.CodeAuthService,
	mongoRepo *storage.MongoRepository,
	loginStateMgr *repository.LoginStateManager,
	permissionsSvc *services.PermissionsService,
) *CodeHandler {
	return &CodeHandler{
		jwtService:     jwtService,
		codeService:    codeService,
		mongoRepo:      mongoRepo,
		loginStateMgr:  loginStateMgr,
		permissionsSvc: permissionsSvc,
	}
}

// CodeCallbackHandler - POST /auth/code/callback
func (h *CodeHandler) CodeCallbackHandler(c *gin.Context) {
	var req models.CodeAuthRequest
	if err := c.ShouldBind(&req); err != nil {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Invalid request format"})
		return
	}

	// Получаем login token по коду
	loginToken, valid := h.codeService.ValidateCode(req.Code)
	if !valid {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Invalid or expired code"})
		return
	}

	//Проверяем существование login_state
	_, exists := h.loginStateMgr.GetFullLoginState(loginToken)
	if !exists {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Invalid or expired login token"})
		return
	}

	email, err := h.jwtService.ValidateRefreshToken(req.RefreshToken)
	if err != nil {
		c.JSON(http.StatusUnauthorized, models.ErrorResponse{Error: "Invalid refresh token"})
		return
	}

	//Получаем пользователя по email
	ctx := c.Request.Context()
	user, err := h.mongoRepo.GetUserByEmail(ctx, email)
	if err != nil {
		c.JSON(http.StatusInternalServerError, models.ErrorResponse{
			Error: "database error",
		})
		return
	}

	if user == nil {
		//Если пользователя нет — создаём
		user, err = h.mongoRepo.CreateUser(ctx, email, "")
		if err != nil {
			c.JSON(http.StatusInternalServerError, models.ErrorResponse{
				Error: "failed to create user",
			})
			return
		}
	}

	// Проверяем блокировку
	if user.IsBlocked {
		h.loginStateMgr.UpdateLoginState(loginToken, "denied", "", "")
		c.JSON(http.StatusForbidden, models.ErrorResponse{Error: "User is blocked"})
		return
	}

	//Генерируем jwt
	perms := h.permissionsSvc.GetUserPermissions(user)
	tokenPair, _ := h.jwtService.GenerateTokenPair(user, perms)
	if err != nil {
		c.JSON(http.StatusInternalServerError, models.ErrorResponse{
			Error: "failed to generate tokens",
		})
		return
	}

	//Сохраняем refresh token
	err = h.mongoRepo.AddRefreshToken(c.Request.Context(), user.ID.Hex(), tokenPair.RefreshToken, time.Now().Add(7*24*time.Hour))
	if err != nil {
		c.JSON(http.StatusInternalServerError, models.ErrorResponse{Error: "failed to save refresh token"})
		return
	}

	//Обновляем login state
	h.loginStateMgr.UpdateLoginState(loginToken, "granted", tokenPair.AccessToken, tokenPair.RefreshToken)
	c.JSON(http.StatusOK, gin.H{"status": "granted"})
}
