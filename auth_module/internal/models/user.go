package models

import (
	"go.mongodb.org/mongo-driver/bson/primitive"
	"time"
)

type UserRole string

const (
	RoleAdmin   UserRole = "admin"
	RoleTeacher UserRole = "teacher"
	RoleStudent UserRole = "student"
)

type User struct {
	ID            primitive.ObjectID `bson:"_id,omitempty" json:"id"`
	Email         string             `bson:"email" json:"email"`
	FullName      string             `bson:"full_name" json:"full_name"`
	DisplayName   string             `bson:"display_name" json:"display_name"`
	Roles         []UserRole         `bson:"roles" json:"roles"`
	IsBlocked     bool               `bson:"is_blocked" json:"is_blocked"`
	RefreshTokens []RefreshToken     `bson:"refresh_tokens" json:"-"`
	CreatedAt     time.Time          `bson:"created_at" json:"created_at"`
	UpdatedAt     time.Time          `bson:"updated_at" json:"updated_at"`
}

type RefreshToken struct {
	Token     string    `bson:"token" json:"token"`
	CreatedAt time.Time `bson:"created_at" json:"created_at"`
	ExpiresAt time.Time `bson:"expires_at" json:"expires_at"`
}

type LoginState struct {
	Token        string    `json:"token"`
	Status       string    `json:"status"` // "pending", "granted", "denied"
	ExpiresAt    time.Time `json:"expires_at"`
	AccessToken  string    `json:"access_token,omitempty"`
	RefreshToken string    `json:"refresh_token,omitempty"`
}

type AuthCode struct {
	Code       string    `json:"code"`
	LoginToken string    `json:"login_token"`
	ExpiresAt  time.Time `json:"expires_at"`
}

type UserResponse struct {
	ID          string    `json:"id"`
	Email       string    `json:"email"`
	FullName    string    `json:"full_name"`
	DisplayName string    `json:"display_name"`
	Roles       []string  `json:"roles"`
	IsBlocked   bool      `json:"is_blocked"`
	CreatedAt   time.Time `json:"created_at"`
}
