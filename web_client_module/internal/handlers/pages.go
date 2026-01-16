package handlers

import (
 	"html/template"
 	"log"
 	"net/http"

 	"github.com/google/uuid"
 	"web_client_module/internal/api"
)

type Handler struct {
 	Templates *template.Template
 	Sessions  map[string]string
}


func NewHandler(t *template.Template) *Handler {
 	return &Handler{
  		Templates: t,
  		Sessions:  make(map[string]string),
 	}
}

//главная страница
func (h *Handler) Index(w http.ResponseWriter, r *http.Request) {
 	sessionCookie, err := r.Cookie("session_id")
	if err != nil {
  		sessionID := uuid.New().String()
  		http.SetCookie(w, &http.Cookie{
   			Name:     "session_id",
   			Value:    sessionID,
   			Path:     "/",
   			HttpOnly: true,
  		})
 	} else {
 	//роль
  		if role, ok := h.Sessions[sessionCookie.Value]; ok {
   			log.Printf("Пользователь сессии %s уже вошел как %s", sessionCookie.Value, role)
  		}
 	}

 	h.Templates.ExecuteTemplate(w, "login.html", nil)
}

//обработка логина
func (h *Handler) Login(w http.ResponseWriter, r *http.Request) {
 	if r.Method == http.MethodPost {
  		err := r.ParseForm()
  		if err != nil {
   			http.Error(w, "Ошибка обработки формы", http.StatusBadRequest)
   			return
  		}

  		role := r.FormValue("role")
  		if api.SendLogin(role) {
   		//сохраняем роль 
   			sessionCookie, err := r.Cookie("session_id")
   			if err != nil {
    			http.Error(w, "Нет сессии", http.StatusBadRequest)
    			return
   			}
   			h.Sessions[sessionCookie.Value] = role

   			http.SetCookie(w, &http.Cookie{
    			Name:  "role",
    			Value: role,
    			Path:  "/",
   			})

   			log.Printf("Пользователь вошёл: sessionID=%s, role=%s", sessionCookie.Value, role)

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

//выход
func (h *Handler) Logout(w http.ResponseWriter, r *http.Request) {
 	sessionCookie, err := r.Cookie("session_id")
 	if err == nil {
  		delete(h.Sessions, sessionCookie.Value)
 	}

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
 	sessionCookie, err := r.Cookie("session_id")
 	if err != nil {
  		http.Redirect(w, r, "/", http.StatusSeeOther)
  		return
 	}

 	role, ok := h.Sessions[sessionCookie.Value]
 	if !ok || role != "студент" {
  		http.Redirect(w, r, "/", http.StatusSeeOther)
  		return
 	}

	h.Templates.ExecuteTemplate(w, "result.html", map[string]string{
  		"Role": role,
 	})
}

// страница тестов (преподаватель)
func (h *Handler) Tests(w http.ResponseWriter, r *http.Request) {
 	sessionCookie, err := r.Cookie("session_id")
 	if err != nil {
  		http.Redirect(w, r, "/", http.StatusSeeOther)
  		return
 	}

 	role, ok := h.Sessions[sessionCookie.Value]
 	if !ok || role != "преподаватель" {
  		http.Redirect(w, r, "/", http.StatusSeeOther)
  		return
 	}

 	h.Templates.ExecuteTemplate(w, "tests.html", map[string]string{
  		"Role": role,
 	})
}