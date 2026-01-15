package handlers

import (
 	"html/template"
 	"log"
 	"net/http"
)

func LoginPage(w http.ResponseWriter, r *http.Request) {
	log.Println("Open login page")
	tmpl, err := template.ParseFiles("templates/login.html")
 	if err != nil {
  		http.Error(w, "Template error", http.StatusInternalServerError)
  		return
 	}
 	tmpl.Execute(w, nil)
}

func TestsPage(w http.ResponseWriter, r *http.Request) {
 	log.Println("Open tests page")
 	tmpl, err := template.ParseFiles("templates/tests.html")
 	if err != nil {
  		http.Error(w, "Template error", http.StatusInternalServerError)
  		return
 	}
	tmpl.Execute(w, nil)
}

func ResultPage(w http.ResponseWriter, r *http.Request) {
 	log.Println("Open result page")
 	tmpl, err := template.ParseFiles("templates/result.html")
 	if err != nil {
  		http.Error(w, "Template error", http.StatusInternalServerError)
  		return
 	}
 	tmpl.Execute(w, nil)
}