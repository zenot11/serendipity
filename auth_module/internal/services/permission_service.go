package services

import (
	"auth/internal/models"
)

type PermissionsService struct{}

func NewPermissionsService() *PermissionsService {
	return &PermissionsService{}
}

func (s *PermissionsService) GetUserPermissions(user *models.User) []string {
	return models.GetPermissionsForRoles(user.Roles)
}
