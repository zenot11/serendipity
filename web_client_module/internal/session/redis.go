package session

import (
 	"context"
 	"encoding/json"
 	"errors"
 	"time"

 	"github.com/redis/go-redis/v9"
)

type Status string

const (
	StatusAnonymous Status = "anonymous"
 	StatusAuthorized Status = "authorized"
)

type Record struct {
 	Status Status json:"status"
 	LoginToken string json:"login_token,omitempty"
 	AccessToken string json:"access_token,omitempty"
 	RefreshToken string json:"refresh_token,omitempty"
}

type RedisStore struct {
 	rdb *redis.Client
 	ttl time.Duration
}

func NewRedisStore(addr, pass string, db int, ttl time.Duration) *RedisStore {
 	rdb := redis.NewClient(&redis.Options{
  		Addr: addr,
  		Password: pass,
  		DB: db,
 	})
 	return &RedisStore{rdb: rdb, ttl: ttl}
}

func (s *RedisStore) Get(ctx context.Context, key string) (*Record, bool, error) {
 	val, err := s.rdb.Get(ctx, key).Result()
 	if errors.Is(err, redis.Nil) {
  		return nil, false, nil
 	}
 	if err != nil {
 		return nil, false, err
 	}

 	var rec Record
 	if err := json.Unmarshal([]byte(val), &rec); err != nil {
  		return nil, false, err
 	}

	return &rec, true, nil
}

func (s *RedisStore) Set(ctx context.Context, key string, rec *Record) error {
 	b, err := json.Marshal(rec)
 	if err != nil {
  		return err
 	}
 	return s.rdb.Set(ctx, key, string(b), s.ttl).Err()
}

func (s *RedisStore) Delete(ctx context.Context, key string) error {
 	return s.rdb.Del(ctx, key).Err()
}