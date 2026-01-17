package storage

import (
	"auth/internal/models"
	"sync"
	"time"
)

type MemoryStore struct {
	mu          sync.RWMutex
	loginStates map[string]models.LoginState
	authCodes   map[string]models.AuthCode
}

func NewMemoryStore() *MemoryStore {
	return &MemoryStore{
		loginStates: make(map[string]models.LoginState),
		authCodes:   make(map[string]models.AuthCode),
	}
}

func (s *MemoryStore) CreateLoginState(token string, expiresAt time.Time) {
	s.mu.Lock()
	defer s.mu.Unlock()

	s.loginStates[token] = models.LoginState{
		Token:     token,
		Status:    "pending",
		ExpiresAt: expiresAt,
	}
}

func (s *MemoryStore) GetLoginState(token string) (models.LoginState, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	state, exists := s.loginStates[token]
	return state, exists
}
func (s *MemoryStore) UpdateLoginState(token string, status string, accessToken string, refreshToken string) bool {
	s.mu.Lock()
	defer s.mu.Unlock()

	state, exists := s.loginStates[token]
	if !exists {
		return false
	}
	state.Status = status
	if accessToken != "" {
		state.AccessToken = accessToken
	}
	if refreshToken != "" {
		state.RefreshToken = refreshToken
	}

	s.loginStates[token] = state
	return true
}

func (s *MemoryStore) CreateAuthCode(code, loginToken string, expiresAt time.Time) {
	s.mu.Lock()
	defer s.mu.Unlock()

	s.authCodes[code] = models.AuthCode{
		Code:       code,
		LoginToken: loginToken,
		ExpiresAt:  expiresAt,
	}
}

func (s *MemoryStore) GetAuthCode(code string) (models.AuthCode, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	authCode, exists := s.authCodes[code]
	return authCode, exists
}

func (s *MemoryStore) DeleteAuthCode(code string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.authCodes, code)
}

func (s *MemoryStore) Cleanup() {
	s.mu.Lock()
	defer s.mu.Unlock()

	now := time.Now()

	// Очистка просроченных состояний входа
	for token, state := range s.loginStates {
		if now.After(state.ExpiresAt) {
			delete(s.loginStates, token)
		}
	}

	// Очистка просроченных кодов
	for code, authCode := range s.authCodes {
		if now.After(authCode.ExpiresAt) {
			delete(s.authCodes, code)
		}
	}
}
