package middleware

import (
 	"context"
 	"net/http"
 	"time"

 	"github.com/google/uuid"
 	"web_client_module/internal/api"
 	"web_client_module/internal/config"
 	"web_client_module/internal/session"
)

type ctxKey string

const (
 	ctxSessionID ctxKey = "session_id"
 	ctxRecord    ctxKey = "record"
)

func SessionID(r *http.Request) string {
 	v := r.Context().Value(ctxSessionID)
 	if v == nil {
  		return ""
 	}
 	return v.(string)
}

func Record(r *http.Request) *session.Record {
 	v := r.Context().Value(ctxRecord)
 	if v == nil {
  		return nil
 	}
 	return v.(*session.Record)
}

func SessionFlow(next http.Handler, store *session.RedisStore, auth *api.AuthClient, cfg *config.Config) http.Handler {
 	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

  		if len(r.URL.Path) >= 8 && r.URL.Path[:8] == "/static/" {
   			next.ServeHTTP(w, r)
   			return
  		}

  		ctx := r.Context()


  		c, err := r.Cookie(cfg.CookieName)
  		if err != nil || c.Value == "" {
   			if r.URL.Path == "/" || r.URL.Path == "/login" {
    			next.ServeHTTP(w, r)
    			return
   			}
   			http.Redirect(w, r, "/", http.StatusSeeOther)
   			return
  		}

  		sid := c.Value


  		rec, ok, err := store.Get(ctx, sid)
  		if err != nil || !ok {
   			if r.URL.Path == "/" || r.URL.Path == "/login" {
    			next.ServeHTTP(w, r)
    			return
   			}
   			http.Redirect(w, r, "/", http.StatusSeeOther)
   			return
  		}


  		if rec.Status == session.StatusAnonymous {
   			if r.URL.Path == "/login" {
    			ctx = context.WithValue(ctx, ctxSessionID, sid)
    			ctx = context.WithValue(ctx, ctxRecord, rec)
    			next.ServeHTTP(w, r.WithContext(ctx))
    			return
   			}



   			check, err := auth.CheckLogin(rec.LoginToken)
   			if err != nil {

    			_ = store.Delete(context.Background(), sid)
    			http.Redirect(w, r, "/", http.StatusSeeOther)
    			return
   			}

   			if check.Status == "success" {
    			rec.Status = session.StatusAuthorized
    			rec.AccessToken = check.AccessToken
    			rec.RefreshToken = check.RefreshToken
    			rec.LoginToken = ""

    			_ = store.Set(context.Background(), sid, rec)
   			} else if check.Status == "denied" {
    			_ = store.Delete(context.Background(), sid)
    			http.Redirect(w, r, "/", http.StatusSeeOther)
    			return
   			} else {
    			http.Redirect(w, r, "/", http.StatusSeeOther)
    			return
   			}
  		}


  		if rec.Status == session.StatusAuthorized && r.URL.Path == "/login" {
   			http.Redirect(w, r, "/", http.StatusSeeOther)
   			return
  		}

  		ctx = context.WithValue(ctx, ctxSessionID, sid)
  		ctx = context.WithValue(ctx, ctxRecord, rec)

  		next.ServeHTTP(w, r.WithContext(ctx))
 	})
}

func SetSessionCookie(w http.ResponseWriter, cfg *config.Config, sid string) {
 	http.SetCookie(w, &http.Cookie{
  		Name:     cfg.CookieName,
  		Value:    sid,
  		Path:     "/",
  		HttpOnly: true,
  		SameSite: http.SameSiteLaxMode,
  		Expires:  time.Now().Add(24 * time.Hour),
  		Secure:   !cfg.DevMode,
 	})
}

func NewSessionID() string {
 	return uuid.NewString()
}