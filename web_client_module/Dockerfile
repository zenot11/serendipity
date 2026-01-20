FROM golang:1.22-alpine AS build

WORKDIR /app

COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN go build -o webclient ./cmd/web

FROM alpine:3.20

WORKDIR /app
COPY --from=build /app/webclient /app/webclient
COPY templates /app/templates
COPY static /app/static

EXPOSE 8080
CMD ["./webclient"]