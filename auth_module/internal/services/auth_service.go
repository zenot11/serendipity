package services

import (
	"auth/internal/storage"
	"crypto/rand"
	"fmt"
	"math/big"
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
	code := s.generateRandomCode(6)

	//Позже сохранение в хранилище
	return code
}

// GenerateRandomCode генерирует случайный цифровой код
func (s *CodeAuthService) generateRandomCode(length int) string {
	code := ""
	for i := 0; i < length; i++ {
		n, err := rand.Int(rand.Reader, big.NewInt(10))
		if err != nil {
			code += fmt.Sprintf("%d", i%10)
		} else {
			code += fmt.Sprintf("%d", n)
		}
	}
	return code
}
