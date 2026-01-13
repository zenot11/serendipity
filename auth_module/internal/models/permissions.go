package models

type Permission string

const (
	PermissionUserListRead Permission = "user:list:read"
	// Остальные константы будут добавлены позже
)

// RolePermissions - разрешения для каждой роли
var RolePermissions = map[UserRole][]Permission{
	RoleStudent: {
		// Студент не имеет специальных разрешений по умолчанию
	},
	RoleTeacher: {
		PermissionUserListRead,
	},
	RoleAdmin: {
		PermissionUserListRead,
	},
}

// GetPermissionsForRoles возвращает все разрешения для ролей пользователя
func GetPermissionsForRoles(roles []UserRole) []string {
	permissions := make([]string, 0)
	seen := make(map[string]bool)

	for _, role := range roles {
		if rolePerms, ok := RolePermissions[role]; ok {
			for _, perm := range rolePerms {
				permStr := string(perm)
				if !seen[permStr] {
					permissions = append(permissions, permStr)
					seen[permStr] = true
				}
			}
		}
	}

	return permissions
}
