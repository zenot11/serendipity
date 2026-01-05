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

func (s *MemoryStore) CreateLoginState(token string, expiresAt time.Time) models.LoginState {
	s.mu.Lock()
	defer s.mu.Unlock()

	state := models.LoginState{
		Token:     token,
		Status:    "pending",
		ExpiresAt: expiresAt,
	}

	s.loginStates[token] = state
	return state
}

func (s *MemoryStore) GetLoginState(token string) (models.LoginState, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	state, exists := s.loginStates[token]
	if !exists {
		return models.LoginState{}, false
	}

	return state, true
}

func (s *MemoryStore) DeleteLoginState(token string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.loginStates, token)
}
