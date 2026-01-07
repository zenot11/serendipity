package storage

import (
	"auth/internal/models"
	"context"
	"time"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
)

type MongoRepository struct {
	client    *mongo.Client
	database  *mongo.Database
	usersColl *mongo.Collection
}

func NewMongoRepository(uri, dbName string) (*MongoRepository, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	client, err := mongo.Connect(ctx, options.Client().ApplyURI(uri))
	if err != nil {
		return nil, err
	}

	err = client.Ping(ctx, nil)
	if err != nil {
		return nil, err
	}

	database := client.Database(dbName)
	usersColl := database.Collection("users")

	return &MongoRepository{
		client:    client,
		database:  database,
		usersColl: usersColl,
	}, nil
}

func (r *MongoRepository) GetUserByEmail(ctx context.Context, email string) (*models.User, error) {
	var user models.User
	err := r.usersColl.FindOne(ctx, bson.M{"email": email}).Decode(&user)
	if err == mongo.ErrNoDocuments {
		return nil, nil
	}
	return &user, err
}

func (r *MongoRepository) CreateUser(ctx context.Context, email, displayName string) (*models.User, error) {
	user := models.User{
		ID:            primitive.NewObjectID(),
		Email:         email,
		FullName:      "",
		DisplayName:   displayName,
		Roles:         []models.UserRole{models.RoleStudent},
		IsBlocked:     false,
		RefreshTokens: nil,
		CreatedAt:     time.Now(),
		UpdatedAt:     time.Time{},
	}

	_, err := r.usersColl.InsertOne(ctx, user)
	if err != nil {
		return nil, err
	}
	return &user, err
}

func (r *MongoRepository) Ping(ctx context.Context) error {
	return r.client.Ping(ctx, nil)
}

func (r *MongoRepository) Close() error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	return r.client.Disconnect(ctx)
}
