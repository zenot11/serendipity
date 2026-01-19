package handlers

import (
 	"html/template"
 	"net/http"
)

type Renderer struct {
 	T *template.Template
}

func (r *Renderer) Render(w http.ResponseWriter, name string, data any) {
 	w.Header().Set("Content-Type", "text/html; charset=utf-8")
 	_ = r.T.ExecuteTemplate(w, name, data)
}