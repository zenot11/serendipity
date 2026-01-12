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

// CreateLoginState создаем новое состояние входа
func (m *LoginStateManager) CreateLoginState(token string) {
	expiresAt := time.Now().Add(m.ttl)
	m.store.CreateLoginState(token, expiresAt)
}

// GetLoginState получаем состояние входа
func (m *LoginStateManager) GetLoginState(token string) (models.LoginState, bool) {
	state, exists := m.store.GetLoginState(token)
	if !exists {
		return models.LoginState{}, false
	}

	// Проверяем срок действия
	if time.Now().After(state.ExpiresAt) {
		return models.LoginState{}, false
	}

	return state, true
}
