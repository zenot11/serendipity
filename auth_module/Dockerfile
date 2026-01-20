FROM golang:1.24-alpine AS build
WORKDIR /app

COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN go build -o auth ./cmd

FROM alpine:3.20
WORKDIR /app
COPY --from=build /app/auth /app/auth

ENV PORT=8081
EXPOSE 8081
CMD ["/app/auth"]
