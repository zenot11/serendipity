package main

import (
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

 	http.HandleFunc("/", h.Index)
 	http.HandleFunc("/login", h.Login)
 	http.HandleFunc("/logout", h.Logout)

 	fs := http.FileServer(http.Dir("static"))
 	http.Handle("/static/", http.StripPrefix("/static/", fs))

 	log.Println("Web-client запущен на http://localhost:8080")
 	err = http.ListenAndServe(":8080", nil)
 	if err != nil {
  		log.Fatal("Ошибка запуска сервера:", err)
 	}
}