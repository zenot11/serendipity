package storage

import (
	"auth/internal/models"
	"context"
	"math/rand"
	"strconv"
	"time"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
)

type MongoRepository struct {
	client    *mongo.Client
	database  *mongo.Database
	UsersColl *mongo.Collection
}

func NewMongoRepository(uri, dbName string) (*MongoRepository, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	clientOptions := options.Client().ApplyURI(uri)
	client, err := mongo.Connect(ctx, clientOptions)
	if err != nil {
		return nil, err
	}

	err = client.Ping(ctx, nil)
	if err != nil {
		return nil, err
	}

	database := client.Database(dbName)
	usersColl := database.Collection("users")

	// Создаем индексы
	_, err = usersColl.Indexes().CreateOne(ctx, mongo.IndexModel{
		Keys:    bson.D{{Key: "email", Value: 1}},
		Options: options.Index().SetUnique(true),
	})
	if err != nil {
		return nil, err
	}

	_, err = usersColl.Indexes().CreateOne(ctx, mongo.IndexModel{
		Keys: bson.D{{Key: "refresh_tokens.token", Value: 1}},
	})
	if err != nil {
		return nil, err
	}

	return &MongoRepository{
		client:    client,
		database:  database,
		UsersColl: usersColl,
	}, nil
}

func (r *MongoRepository) GetUserByEmail(ctx context.Context, email string) (*models.User, error) {
	var user models.User
	err := r.UsersColl.FindOne(ctx, bson.M{"email": email}).Decode(&user)
	if err != nil {
		if err == mongo.ErrNoDocuments {
			return nil, nil
		}
		return nil, err
	}
	return &user, nil
}

func (r *MongoRepository) GetUserByID(ctx context.Context, id string) (*models.User, error) {
	objID, err := primitive.ObjectIDFromHex(id)
	if err != nil {
		return nil, err
	}

	var user models.User
	err = r.UsersColl.FindOne(ctx, bson.M{"_id": objID}).Decode(&user)
	if err != nil {
		if err == mongo.ErrNoDocuments {
			return nil, nil
		}
		return nil, err
	}
	return &user, nil
}

func (r *MongoRepository) CreateUser(ctx context.Context, email, fullName string) (*models.User, error) {
	// Счетчик для "Аноним+номер"
	displayName := "Аноним" + strconv.Itoa(rand.Intn(10000))
	if fullName == "" {
		fullName = ""
	}
	user := models.User{
		ID:            primitive.NewObjectID(),
		Email:         email,
		FullName:      fullName,
		DisplayName:   displayName,
		Roles:         []models.UserRole{models.RoleStudent},
		IsBlocked:     false,
		RefreshTokens: nil,
		CreatedAt:     time.Now(),
		UpdatedAt:     time.Now(),
	}

	_, err := r.UsersColl.InsertOne(ctx, user)
	return &user, err
}

func (r *MongoRepository) AddRefreshToken(ctx context.Context, userID, refreshToken string, expiresAt time.Time) error {
	objID, err := primitive.ObjectIDFromHex(userID)
	if err != nil {
		return err
	}

	refreshTokenDoc := models.RefreshToken{
		Token:     refreshToken,
		CreatedAt: time.Now(),
		ExpiresAt: expiresAt,
	}

	_, err = r.UsersColl.UpdateOne(
		ctx,
		bson.M{"_id": objID},
		bson.M{
			"$push": bson.M{"refresh_tokens": refreshTokenDoc},
			"$set":  bson.M{"updated_at": time.Now()},
		},
	)
	return err
}

func (r *MongoRepository) RemoveRefreshToken(ctx context.Context, refreshToken string) error {
	_, err := r.UsersColl.UpdateOne(
		ctx,
		bson.M{"refresh_tokens.token": refreshToken},
		bson.M{
			"$pull": bson.M{"refresh_tokens": bson.M{"token": refreshToken}},
			"$set":  bson.M{"updated_at": time.Now()},
		},
	)
	return err
}

func (r *MongoRepository) HasRefreshToken(ctx context.Context, refreshToken string) (bool, error) {
	count, err := r.UsersColl.CountDocuments(ctx, bson.M{"refresh_tokens.token": refreshToken})
	if err != nil {
		return false, err
	}
	return count > 0, nil
}

func (r *MongoRepository) Close() error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	return r.client.Disconnect(ctx)
}

func (r *MongoRepository) Ping(ctx context.Context) error {
	return r.client.Ping(ctx, nil)
}
