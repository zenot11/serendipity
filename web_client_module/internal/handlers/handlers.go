package handlers

import (
	"context"
	"html/template"
	"net/http"

	"github.com/google/uuid"
	"web_client_module/internal/api"
	"web_client_module/internal/config"
	"web_client_module/internal/middleware"
	"web_client_module/internal/session"
)

type Handler struct {
	R     *Renderer
	Store *session.RedisStore
	Auth  *api.AuthClient
	Main  *api.MainClient
	Cfg   *config.Config
}

func New(tmpl *template.Template, store *session.RedisStore, auth *api.AuthClient, main *api.MainClient, cfg *config.Config) *Handler {
	return &Handler{
		R:     &Renderer{T: tmpl},
		Store: store,
		Auth:  auth,
		Main:  main,
		Cfg:   cfg,
	}
}

type actionItem struct {
	Title  string
	Method string
	Path   string
	Body   string
}

func (h *Handler) Home(w http.ResponseWriter, r *http.Request) {
	rec := middleware.Record(r)

	//unknown
	if rec == nil {
		h.R.Render(w, "index_unknown.html", map[string]any{})
		return
	}

	//anonymous
	if rec.Status == session.StatusAnonymous {
		h.R.Render(w, "index_anonymous.html", map[string]any{
			"LoginToken": rec.LoginToken,
		})
		return
	}

	//authorized
	if rec.Status == session.StatusAuthorized {
		h.R.Render(w, "index_authorized.html", map[string]any{
			"AccessToken": rec.AccessToken,
		})
		return
	}

	http.Redirect(w, r, "/", http.StatusSeeOther)
}

func (h *Handler) Login(w http.ResponseWriter, r *http.Request) {
	loginType := r.URL.Query().Get("type")
	if loginType == "" {
		http.Redirect(w, r, "/", http.StatusSeeOther)
		return
	}

	//session id
	sid := middleware.SessionID(r)
	if sid == "" {
		sid = middleware.NewSessionID()
		middleware.SetSessionCookie(w, h.Cfg, sid)
	}

	loginToken := uuid.NewString()

	rec := &session.Record{
		Status:     session.StatusAnonymous,
		LoginToken: loginToken,
	}
	_ = h.Store.Set(context.Background(), sid, rec)

	startResp, err := h.Auth.StartLogin(loginType, loginToken)
	if err != nil {
		h.R.Render(w, "error.html", map[string]any{
			"Title":   "Ошибка входа",
			"Message": err.Error(),
		})
		return
	}

	if startResp.Type == "code" && startResp.Code != "" {
		h.R.Render(w, "auth_code.html", map[string]any{
			"Code": startResp.Code,
		})
		return
	}

	if startResp.URL != "" {
		http.Redirect(w, r, startResp.URL, http.StatusSeeOther)
		return
	}

	h.R.Render(w, "error.html", map[string]any{
		"Title":   "Ошибка",
		"Message": "Пустой ответ от сервера авторизации",
	})
}

func (h *Handler) Logout(w http.ResponseWriter, r *http.Request) {
	rec := middleware.Record(r)
	sid := middleware.SessionID(r)

	if sid != "" {
		_ = h.Store.Delete(context.Background(), sid)
	}

	if r.URL.Query().Get("all") == "true" && rec != nil && rec.RefreshToken != "" {
		_ = h.Auth.LogoutAll(rec.RefreshToken)
	}

	http.Redirect(w, r, "/", http.StatusSeeOther)
}

func (h *Handler) ActionsPage(w http.ResponseWriter, r *http.Request) {
	rec := middleware.Record(r)
	if rec == nil || rec.Status != session.StatusAuthorized {
		http.Redirect(w, r, "/", http.StatusSeeOther)
		return
	}

	items := []actionItem{
		{Title: "Demo OK (200)", Method: "GET", Path: "/demo/ok", Body: ""},
		{Title: "Demo Forbidden (403)", Method: "GET", Path: "/demo/403", Body: ""},
		{Title: "Demo Expired token (401 + refresh)", Method: "GET", Path: "/demo/401", Body: ""},
	}

	h.R.Render(w, "actions.html", map[string]any{
		"Actions": items,
	})
}

func (h *Handler) DoAction(w http.ResponseWriter, r *http.Request) {
	rec := middleware.Record(r)
	sid := middleware.SessionID(r)

	if rec == nil || rec.Status != session.StatusAuthorized {
		http.Redirect(w, r, "/", http.StatusSeeOther)
		return
	}

	_ = r.ParseForm()
	method := r.FormValue("method")
	path := r.FormValue("path")
	body := r.FormValue("body")

	status, respBody, err := h.Main.Do(method, path, rec.AccessToken, []byte(body))
	if err != nil {
		h.R.Render(w, "action_result.html", map[string]any{
			"Title":  "Ошибка",
			"Status": "CLIENT ERROR",
			"Body":   err.Error(),
		})
		return
	}

	if status == 403 {
		h.R.Render(w, "action_result.html", map[string]any{
			"Title":  "Нет доступа",
			"Status": "403 FORBIDDEN",
			"Body":   respBody,
		})
		return
	}

	//refresh при 401
	if status == 401 {
		newPair, err := h.Auth.Refresh(rec.RefreshToken)
		if err != nil {
			_ = h.Store.Delete(context.Background(), sid)
			http.Redirect(w, r, "/", http.StatusSeeOther)
			return
		}

		rec.AccessToken = newPair.AccessToken
		rec.RefreshToken = newPair.RefreshToken
		_ = h.Store.Set(context.Background(), sid, rec)

		status2, body2, _ := h.Main.Do(method, path, rec.AccessToken, []byte(body))

		h.R.Render(w, "action_result.html", map[string]any{
			"Title":  "Ответ (после refresh)",
			"Status": status2,
			"Body":   body2,
		})
		return
	}

	h.R.Render(w, "action_result.html", map[string]any{
		"Title":  "Ответ",
		"Status": status,
		"Body":   respBody,
	})
}