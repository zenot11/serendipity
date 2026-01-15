package handlers

import (
	"auth/internal/database/repository"
	"auth/internal/database/storage"
	"auth/internal/pkg/oauth"
	"net/http"

	"github.com/gin-gonic/gin"
)

type OAuthHandler struct {
	oauthService  *oauth.OAuthService
	loginStateMgr *repository.LoginStateManager
	mongoRepo     *storage.MongoRepository
}

func NewOAuthHandler(oauthService *oauth.OAuthService, loginStateMgr *storage.MongoRepository, mongoRepo *repository.LoginStateManager) *OAuthHandler {
	return &OAuthHandler{
		oauthService:  oauthService,
		loginStateMgr: loginStateMgr,
		mongoRepo:     mongoRepo,
	}
}

// GitHubCallbackHandler - GET /auth/github/callback
func (h *OAuthHandler) GitHubCallbackHandler(c *gin.Context) {
	h.handleOAuthCallback(c, "github")
}

// YandexCallbackHandler - GET /auth/yandex/callback
func (h *OAuthHandler) YandexCallbackHandler(c *gin.Context) {
	h.handleOAuthCallback(c, "yandex")
}

// handleOAuthCallback обрабатывает OAuth callback
func (h *OAuthHandler) handleOAuthCallback(c *gin.Context, provider string) {
	code := c.Query("code")
	state := c.Query("state")
	errorParam := c.Query("error")

	// Если пользователь отказался
	if errorParam != "" {
		h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
		//
		return
	}

	if code == "" || state == "" {
		h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
		//
		return
	}

	// Возвращаем страницу успеха
	c.Redirect(http.StatusFound, "/")
}
