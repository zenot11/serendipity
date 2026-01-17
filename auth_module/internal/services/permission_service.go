package services

import (
	"auth/internal/models"
)

type PermissionsService struct{}

func NewPermissionsService() *PermissionsService {
	return &PermissionsService{}
}

// GetUserPermissions возвращает разрешения пользователя
func (s *PermissionsService) GetUserPermissions(user *models.User) []string {
	return models.GetPermissionsForRoles(user.Roles)
}

// HasPermission проверяет наличие разрешения
func (s *PermissionsService) HasPermission(user *models.User, permission string) bool {
	permissions := models.GetPermissionsForRoles(user.Roles)
	for _, p := range permissions {
		if p == permission {
			return true
		}
	}
	return false
}
