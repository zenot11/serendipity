package main

import (
 	"log"
 	"net/http"

 	"web-client/internal/handlers"
)

func main() {
 	mux := http.NewServeMux()

 	mux.HandleFunc("/login", handlers.LoginPage)
 	mux.HandleFunc("/tests", handlers.TestsPage)
 	mux.HandleFunc("/result", handlers.ResultPage)

 	fs := http.FileServer(http.Dir("./static"))
 	mux.Handle("/static/", http.StripPrefix("/static/", fs))

 	log.Println("Server started on http://localhost:8080")

 	err := http.ListenAndServe(":8080", mux)
 		if err != nil {
  		log.Fatal(err)
 	}
}