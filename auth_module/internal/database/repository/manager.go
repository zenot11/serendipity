package repository

import (
	"auth/internal/database/storage"
	"auth/internal/models"
	"time"
)

type LoginStateManager struct {
	store *storage.MemoryStore
	ttl   time.Duration
}

func NewLoginStateManager(store *storage.MemoryStore, ttl time.Duration) *LoginStateManager {
	return &LoginStateManager{
		store: store,
		ttl:   ttl,
	}
}

// CreateLoginState создаёт новое состояние входа
func (m *LoginStateManager) CreateLoginState(token string) {
	expiresAt := time.Now().Add(m.ttl)
	m.store.CreateLoginState(token, expiresAt)
}

// GetFullLoginState получает состояние входа
func (m *LoginStateManager) GetFullLoginState(token string) (models.LoginState, bool) {
	state, exists := m.store.GetLoginState(token)
	if !exists || time.Now().After(state.ExpiresAt) {
		return models.LoginState{}, false
	}
	return state, true
}

// UpdateLoginState обновляет состояние входа
func (m *LoginStateManager) UpdateLoginState(token, status, accessToken, refreshToken string) {
	m.store.UpdateLoginState(token, status, accessToken, refreshToken)
}
