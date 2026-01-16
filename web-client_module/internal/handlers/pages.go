package handlers

import (
 	"html/template"
 	"net/http"

 	"github.com/google/uuid"
)

type Handler struct {
 	Templates *template.Template
}

// обработчк
func NewHandler(t *template.Template) *Handler {
 	return &Handler{
  		Templates: t,
 	}
}

// главная страница
func (h *Handler) Index(w http.ResponseWriter, r *http.Request) {
 	// проверяем cookie 
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

 	err = h.Templates.ExecuteTemplate(w, "login.html", nil)
 	if err != nil {
  		http.Error(w, "Ошибка отображения страницы", http.StatusInternalServerError)
 	}
}

// страница логина
func (h *Handler) Login(w http.ResponseWriter, r *http.Request) {
 	err := h.Templates.ExecuteTemplate(w, "login.html", nil)
 	if err != nil {
  		http.Error(w, "Ошибка отображения страницы", http.StatusInternalServerError)
 	}
}

// выход !!!заглушка!!!
func (h *Handler) Logout(w http.ResponseWriter, r *http.Request) {
 	http.Redirect(w, r, "/", http.StatusSeeOther)
}