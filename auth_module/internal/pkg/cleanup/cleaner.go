package cleanup

import (
	"auth/internal/database/storage"
	"time"
)

type CleanupScheduler struct {
	store    *storage.MemoryStore
	interval time.Duration
}

func NewCleanupScheduler(store *storage.MemoryStore, interval time.Duration) *CleanupScheduler {
	return &CleanupScheduler{
		store:    store,
		interval: interval,
	}
}

// Start запускает фоновую очистку
func (s *CleanupScheduler) Start() {
	go func() {
		ticker := time.NewTicker(s.interval)
		defer ticker.Stop()

		for range ticker.C {
			s.store.Cleanup()
		}
	}()
}
