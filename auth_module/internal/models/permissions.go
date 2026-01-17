package models

type Permission string

const (
	// User permissions
	PermissionUserListRead      Permission = "user:list:read"
	PermissionUserFullNameWrite Permission = "user:fullName:write"
	PermissionUserDataRead      Permission = "user:data:read"
	PermissionUserRolesRead     Permission = "user:roles:read"
	PermissionUserRolesWrite    Permission = "user:roles:write"
	PermissionUserBlockRead     Permission = "user:block:read"
	PermissionUserBlockWrite    Permission = "user:block:write"

	// Course permissions
	PermissionCourseInfoWrite Permission = "course:info:write"
	PermissionCourseTestList  Permission = "course:testList"
	PermissionCourseTestRead  Permission = "course:test:read"
	PermissionCourseTestWrite Permission = "course:test:write"
	PermissionCourseTestAdd   Permission = "course:test:add"
	PermissionCourseTestDel   Permission = "course:test:del"
	PermissionCourseUserList  Permission = "course:userList"
	PermissionCourseUserAdd   Permission = "course:user:add"
	PermissionCourseUserDel   Permission = "course:user:del"
	PermissionCourseAdd       Permission = "course:add"
	PermissionCourseDel       Permission = "course:del"

	// Question permissions
	PermissionQuestListRead Permission = "quest:list:read"
	PermissionQuestRead     Permission = "quest:read"
	PermissionQuestUpdate   Permission = "quest:update"
	PermissionQuestCreate   Permission = "quest:create"
	PermissionQuestDel      Permission = "quest:del"

	// Test permissions
	PermissionTestQuestDel    Permission = "test:quest:del"
	PermissionTestQuestAdd    Permission = "test:quest:add"
	PermissionTestQuestUpdate Permission = "test:quest:update"
	PermissionTestAnswerRead  Permission = "test:answer:read"

	// Answer permissions
	PermissionAnswerRead   Permission = "answer:read"
	PermissionAnswerUpdate Permission = "answer:update"
	PermissionAnswerDel    Permission = "answer:del"
)

// RolePermissions - разрешения для каждой роли
var RolePermissions = map[UserRole][]Permission{
	RoleStudent: {
		// Студент не имеет специальных разрешений по умолчанию
	},
	RoleTeacher: {
		PermissionCourseInfoWrite,
		PermissionCourseTestList,
		PermissionCourseTestRead,
		PermissionCourseTestWrite,
		PermissionCourseTestAdd,
		PermissionCourseTestDel,
		PermissionCourseUserList,
		PermissionCourseUserAdd,
		PermissionCourseUserDel,
		PermissionCourseDel,
		PermissionQuestListRead,
		PermissionQuestRead,
		PermissionQuestUpdate,
		PermissionQuestCreate,
		PermissionQuestDel,
		PermissionTestQuestDel,
		PermissionTestQuestAdd,
		PermissionTestQuestUpdate,
		PermissionTestAnswerRead,
	},
	RoleAdmin: {
		PermissionUserListRead,
		PermissionUserFullNameWrite,
		PermissionUserDataRead,
		PermissionUserRolesRead,
		PermissionUserRolesWrite,
		PermissionUserBlockRead,
		PermissionUserBlockWrite,
		PermissionCourseAdd,
		PermissionCourseDel,
		PermissionQuestListRead,
		PermissionQuestRead,
		PermissionQuestUpdate,
		PermissionQuestCreate,
		PermissionQuestDel,
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
