package api

import (
 	"log"
)

func SendLogin(role string) bool {
 	log.Println("Попытка входа с ролью:", role)

	switch role {
	case "студент", "преподаватель", "администратор":
		return true
	default:
		return false
	}
}