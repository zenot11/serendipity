package handlers

import (
 	"html/template"
 	"net/http"

 	"github.com/google/uuid"
 	"web_client_module/internal/api"
)

type Handler struct {
 	Templates *template.Template
}


func NewHandler(t *template.Template) *Handler {
 	return &Handler{
  		Templates: t,
 	}
}

//главная страница
func (h *Handler) Index(w http.ResponseWriter, r *http.Request) {
	_, err := r.Cookie("session_id")
 	if err != nil {
  		sessionID := uuid.New().String()
  		http.SetCookie(w, &http.Cookie{
  			 Name:     "session_id",
   			Value:    sessionID,
   			Path:     "/",
   			HttpOnly: true,
  		})
 	}

 	h.Templates.ExecuteTemplate(w, "login.html", nil)
}

// обработка логина через форму
func (h *Handler) Login(w http.ResponseWriter, r *http.Request) {
 	if r.Method == http.MethodPost {
  		err := r.ParseForm()
  		if err != nil {
   			http.Error(w, "Ошибка обработки формы", http.StatusBadRequest)
   			return
  		}

 		role := r.FormValue("role")
  		if api.SendLogin(role) {
   			http.SetCookie(w, &http.Cookie{
    			Name:  "role",
    			Value: role,
    			Path:  "/",
   			})
   			switch role {
   			case "студент":
    			http.Redirect(w, r, "/result", http.StatusSeeOther)
   			case "преподаватель":
   				 http.Redirect(w, r, "/tests", http.StatusSeeOther)
   			default:
   				 http.Redirect(w, r, "/", http.StatusSeeOther)
   			}
   			return
  		}
 	}

 	h.Templates.ExecuteTemplate(w, "login.html", nil)
}

// выход
func (h *Handler) Logout(w http.ResponseWriter, r *http.Request) {
 // удаляем cookie
 	http.SetCookie(w, &http.Cookie{
  		Name:   "role",
  		Value:  "",
  		Path:   "/",
  		MaxAge: -1,
 	})
 	http.Redirect(w, r, "/", http.StatusSeeOther)
}

// страница результатов (студент)
func (h *Handler) Result(w http.ResponseWriter, r *http.Request) {
 	roleCookie, err := r.Cookie("role")
 	if err != nil || roleCookie.Value != "студент" {
  		http.Redirect(w, r, "/", http.StatusSeeOther)
  		return
 	}

 	h.Templates.ExecuteTemplate(w, "result.html", map[string]string{
  		"Role": roleCookie.Value,
 	})
}

// страница тестов (преподаватель)
func (h *Handler) Tests(w http.ResponseWriter, r *http.Request) {
 	roleCookie, err := r.Cookie("role")
 	if err != nil || roleCookie.Value != "преподаватель" {
  		http.Redirect(w, r, "/", http.StatusSeeOther)
  		return
 	}

 	h.Templates.ExecuteTemplate(w, "tests.html", map[string]string{
  		"Role": roleCookie.Value,
 	})
}