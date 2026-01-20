package main

import (
	"html/template"
	"log"
	"net/http"
	"path/filepath"
	"time"

	"web_client_module/internal/api"
	"web_client_module/internal/config"
	"web_client_module/internal/handlers"
	"web_client_module/internal/middleware"
	"web_client_module/internal/session"
)

func main() {
	cfg := config.MustLoad()

	tmpl, err := template.ParseGlob(filepath.Join("templates", "*.html"))
	if err != nil {
		log.Fatalf("Ошибка загрузки шаблонов: %v", err)
	}

	store := session.NewRedisStore(cfg.RedisAddr, cfg.RedisPassword, cfg.RedisDB, cfg.SessionTTL)

	authClient := api.NewAuthClient(cfg.AuthURL, cfg.Mock)
	mainClient := api.NewMainClient(cfg.MainURL, cfg.Mock)

	h := handlers.New(tmpl, store, authClient, mainClient, cfg)

	mux := http.NewServeMux()

	fs := http.FileServer(http.Dir("static"))
	mux.Handle("/static/", http.StripPrefix("/static/", fs))

	mux.HandleFunc("/", h.Home)
	mux.HandleFunc("/login", h.Login)
	mux.HandleFunc("/logout", h.Logout)

	mux.HandleFunc("/actions", h.ActionsPage)
	mux.HandleFunc("/actions/do", h.DoAction)

	var handler http.Handler = mux
	handler = middleware.Logging(handler)
	handler = middleware.SessionFlow(handler, store, authClient, cfg)

	srv := &http.Server{
		Addr:         ":" + cfg.Port,
		Handler:      handler,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 15 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	log.Printf("Web-client запущен: http://localhost:%s", cfg.Port)
	log.Fatal(srv.ListenAndServe())
}
