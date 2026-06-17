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

void controls(Camera3D *camera, float speed, float rotSpeed);

int main()
{
    InitWindow(W, H, "Horror game");

    // Define the camera to look into our 3D world
    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 2.0f, 5.0f};
    camera.target = (Vector3){0.0f, 10.0f, 2.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

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

        // draw
        BeginMode3D(camera);

        DrawCube(cubePosition, 2.0f, 2.0f, 2.0f, RED);
        DrawCubeWires(cubePosition, 2.0f, 2.0f, 2.0f, MAROON);

        DrawGrid(100, 1.0f); // int slices, float spacing

        EndMode3D();

        DrawText("FPS the 3rd dimension!", 10, 40, 20, DARKGRAY);

        DrawFPS(10, 10);

        controls(&camera, 0.1f, 1.5f);

        EndDrawing();
    }

    // De Initialization
    CloseWindow(); // close window and openGL context

    return 0;
}

void controls(Camera3D *camera, float speed, float rotSpeed)
{

    float dt = GetFrameTime();
    float angleChange = 0.0f;

    // Get camera facing direction and right vector
    Vector3 forward = {
        camera->target.x - camera->position.x,
        0.0f,
        camera->target.z - camera->position.z};

    // normalize forward vector
    float length = sqrtf(forward.x * forward.x + forward.z * forward.z);
    if (length > 0.0f)
    {
        forward.x /= length;
        forward.z /= length;
    }

    // Right vector (perpendicular to forward)
    Vector3 right = {forward.z, 0.0f, -forward.x};

    // Update position
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
        camera->position.x += right.x * speed/3.0f;
        camera->position.z += right.z * speed/3.0f;
        camera->target.x += right.x * speed/3.0f;
        camera->target.z += right.z * speed/3.0f;
    }
    if (IsKeyDown(KEY_D))
    {
        camera->position.x -= right.x * speed/3.0f;
        camera->position.z -= right.z * speed/3.0f;
        camera->target.x -= right.x * speed/3.0f;
        camera->target.z -= right.z * speed/3.0f;
    }

    if (angleChange != 0.0f)
    {
        // calculate current forward vector
        Vector3 forward = Vector3Subtract(camera->target, camera->position);

        // create a rotation matrix around the Y axis
        Matrix rotationMatrix = MatrixRotateY(angleChange);

        // rotate the forward vector using the matrix
        Vector3 rotatedForward = Vector3Transform(forward, rotationMatrix);

        // update target relative to position
        camera->target = Vector3Add(camera->position, rotatedForward);
    }
}