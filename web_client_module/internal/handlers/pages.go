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
 	Sessions  map[string]string   //session id - роль
 	TestList  map[string][]string //роль - список тестов
}


func NewHandler(t *template.Template) *Handler {
 	return &Handler{
  		Templates: t,
  		Sessions:  make(map[string]string),
  		TestList: map[string][]string{
   			"студент": {},
   			"преподаватель": {},
  		},
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

  		if role, ok := h.Sessions[sessionCookie.Value]; ok {
   			log.Printf("Пользователь сессии %s уже вошел как %s", sessionCookie.Value, role)
  		}
 	}

 	h.Templates.ExecuteTemplate(w, "login.html", nil)
}

//обработка логина через форму
func (h *Handler) Login(w http.ResponseWriter, r *http.Request) {
 	if r.Method == http.MethodPost {
  		err := r.ParseForm()
  		if err != nil {
   			http.Error(w, "Ошибка обработки формы", http.StatusBadRequest)
  			return
  		}

  		role := r.FormValue("role")
  		if api.SendLogin(role) {
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

//результаты студент
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

 	data := map[string]interface{}{
  		"Role": role,
  		"Message": "",
 	}

 	if r.Method == http.MethodPost {
  		r.ParseForm()
  		testName := r.FormValue("test")
  		if testName != "" {
   			data["Message"] = "Вы выбрали тест: " + testName
  		}
 	}

 	h.Templates.ExecuteTemplate(w, "result.html", data)
}

//тесты преподаватель
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

 	if r.Method == http.MethodPost {
 		r.ParseForm()
  		newTest := r.FormValue("newtest")
  		if newTest != "" {
   			h.TestList["преподаватель"] = append(h.TestList["преподаватель"], newTest)
  		}
 	}

 	data := map[string]interface{}{
  		"Role":  role,
  		"Tests": h.TestList["преподаватель"],
 	}

 	h.Templates.ExecuteTemplate(w, "tests.html", data)
}