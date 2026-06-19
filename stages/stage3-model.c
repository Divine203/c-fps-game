#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include <math.h>
#include <raymath.h>

const int W = 1500;
const int H = 950;

const Color bgColor = {0, 0, 0, 0};

double viewBobTime = 0.0f;

void controls(Camera3D *camera, float speed, float rotSpeed);
Vector3 applyViewBob(double dt);
bool isMoving = false;

Camera3D camera = {0};

int main()
{
    InitWindow(W, H, "Horror game");

    camera.position = (Vector3){0.0f, 2.0f, 5.0f};
    camera.target = (Vector3){0.0f, 10.0f, 2.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Model model = LoadModel("assets/dungeon+assets.glb");
    BoundingBox bounds = GetMeshBoundingBox(model.meshes[0]);

    Vector3 cubePosition = {0.0f, 1.0f, 0.0f};

    int cameraMode = CAMERA_FIRST_PERSON;

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, cameraMode);

        if (IsKeyPressed(KEY_Z))
            camera.target = (Vector3){0.0f, 0.0f, 0.0f};

        BeginDrawing();
        ClearBackground(bgColor);

        BeginMode3D(camera);

        DrawModel(model, cubePosition, 1.0F, WHITE);

        // DrawCube(cubePosition, 2.0f, 2.0f, 2.0f, RED);
        // DrawCubeWires(cubePosition, 2.0f, 2.0f, 2.0f, MAROON);

        DrawGrid(100, 1.0f); // int slices, float spacing

        EndMode3D();

        DrawText("FPS the 3rd dimension!", 10, 40, 20, DARKGRAY);

        DrawFPS(10, 10);

        controls(&camera, 0.1f, 1.5f);

        EndDrawing();
    }
    
    CloseWindow();

    return 0;
}

Vector3 applyViewBob(double dt)
{
    float bobAmount = 0.015f;
    float swayAmount = 0.03f;

    if (isMoving)
    {
        viewBobTime += (dt * 8.0);
    }
    else
    {
        viewBobTime = 0;
        camera.position.y = 2.0f;
    }

    float verticalBob = sin(viewBobTime) * bobAmount;
    float sidewaysSway = cos(viewBobTime * 0.5f) * swayAmount;

    Vector3 renderPos = camera.position;

    renderPos.y += verticalBob;
    renderPos.x += sidewaysSway;

    return renderPos;
}

void controls(Camera3D *camera, float speed, float rotSpeed)
{
    float dt = GetFrameTime();
    float angleChange = 0.0f;

    Vector3 forward = {
        camera->target.x - camera->position.x,
        0.0f,
        camera->target.z - camera->position.z};

    float length = sqrtf(forward.x * forward.x + forward.z * forward.z);
    if (length > 0.0f)
    {
        forward.x /= length;
        forward.z /= length;
    }

    Vector3 right = {forward.z, 0.0f, -forward.x};

    Vector3 renderPos = applyViewBob(dt);

    if (isMoving)
    {
        camera->position = renderPos;
    }

    if (IsKeyDown(KEY_LEFT))
    {
        angleChange += rotSpeed * dt;
    };
    if (IsKeyDown(KEY_RIGHT))
    {
        angleChange -= rotSpeed * dt;
    };
    if (IsKeyDown(KEY_UP))
    {
        camera->position.x += forward.x * speed;
        camera->position.z += forward.z * speed;
        camera->target.x += forward.x * speed;
        camera->target.z += forward.z * speed;
    };
    if (IsKeyDown(KEY_DOWN))
    {
        camera->position.x -= forward.x * speed;
        camera->position.z -= forward.z * speed;
        camera->target.x -= forward.x * speed;
        camera->target.z -= forward.z * speed;
    };
    if (IsKeyDown(KEY_A))
    {
        camera->position.x += right.x * speed / 3.0f;
        camera->position.z += right.z * speed / 3.0f;
        camera->target.x += right.x * speed / 3.0f;
        camera->target.z += right.z * speed / 3.0f;
    }
    if (IsKeyDown(KEY_D))
    {
        camera->position.x -= right.x * speed / 3.0f;
        camera->position.z -= right.z * speed / 3.0f;
        camera->target.x -= right.x * speed / 3.0f;
        camera->target.z -= right.z * speed / 3.0f;
    }

    isMoving = (IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D));

    if (angleChange != 0.0f)
    {
        Vector3 forward = Vector3Subtract(camera->target, camera->position);

        Matrix rotationMatrix = MatrixRotateY(angleChange);

        Vector3 rotatedForward = Vector3Transform(forward, rotationMatrix);

        camera->target = Vector3Add(camera->position, rotatedForward);
    }
}