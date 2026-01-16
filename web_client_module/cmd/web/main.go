package main

import (
	"fmt"
 	"html/template"
 	"log"
 	"net/http"
 	"path/filepath"

 	"web_client_module/internal/handlers"
)

func main() {
 	templatesPath := filepath.Join("templates", "*.html")
 	tmpl, err := template.ParseGlob(templatesPath)
 	if err != nil {
  		log.Fatal("Ошибка загрузки шаблонов:", err)
 	}

	h := handlers.NewHandler(tmpl)
	
	mux := http.NewServeMux()
 	mux.HandleFunc("/", logRequest(h.Index))
 	mux.HandleFunc("/login", logRequest(h.Login))
 	mux.HandleFunc("/logout", logRequest(h.Logout))
	mux.HandleFunc("/result", logRequest(h.Result))
	mux.HandleFunc("/tests", logRequest(h.Tests))

 	fs := http.FileServer(http.Dir("static"))
 	mux.Handle("/static/", http.StripPrefix("/static/", fs))

 	fmt.Println("Web-client запущен на http://localhost:8080")
 	err = http.ListenAndServe(":8080", mux)
 	if err != nil {
  		log.Fatal("Ошибка запуска сервера:", err)
 	}
}

func logRequest(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		log.Printf("%s %s", r.Method, r.URL.Path)
		next(w, r)
	}
}