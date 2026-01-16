package services

import (
	"auth/internal/database/storage"

	"fmt"
	"math/rand/v2"
	"time"
)

type CodeAuthService struct {
	store   *storage.MemoryStore
	codeTTL time.Duration
}

func NewCodeAuthService(store *storage.MemoryStore, codeTTL time.Duration) *CodeAuthService {
	return &CodeAuthService{
		store:   store,
		codeTTL: codeTTL,
	}
}

// GenerateCode генерирует 6-значный код
func (s *CodeAuthService) GenerateCode(loginToken string) string {
	code := fmt.Sprintf("%06d", rand.IntN(900000)+100000)
	expiresAt := time.Now().Add(s.codeTTL)
	s.store.CreateAuthCode(code, loginToken, expiresAt)
	return code
}

// ValidateCode проверяет код
func (s *CodeAuthService) ValidateCode(code string) (string, bool) {
	authCode, exists := s.store.GetAuthCode(code)
	if !exists || time.Now().After(authCode.ExpiresAt) {
		return "", false
	}

	s.store.DeleteAuthCode(code)
	return authCode.LoginToken, true
}
