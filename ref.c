#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include <math.h>
#include <raymath.h>

#if defined(PLATFORM_DESKTOP) || defined(_WIN32)
#define GLSL_VERSION 330
#else // PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION 100
#endif

#define MAX_LIGHTS 4

// Light type
typedef enum {
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT,
    LIGHT_SPOT
} LightType;

// Light data
typedef struct {
    int type;
    int enabled;
    Vector3 position;
    Vector3 target;
    Color color;
    float intensity;
    bool active;

    // Shader light parameters locations
    int typeLoc;
    int enabledLoc;
    int positionLoc;
    int targetLoc;
    int colorLoc;
    int intensityLoc;
} Light;

static int lightCount = 0;

static void UpdateLight(Shader shader, Light light)
{
    if (!light.active) return;

    SetShaderValue(shader, light.enabledLoc, &light.enabled, SHADER_UNIFORM_INT);
    SetShaderValue(shader, light.typeLoc, &light.type, SHADER_UNIFORM_INT);
    
    float position[3] = { light.position.x, light.position.y, light.position.z };
    SetShaderValue(shader, light.positionLoc, position, SHADER_UNIFORM_VEC3);

    float target[3] = { light.target.x, light.target.y, light.target.z };
    SetShaderValue(shader, light.targetLoc, target, SHADER_UNIFORM_VEC3);

    float color[4] = {
        (float)light.color.r/255.0f,
        (float)light.color.g/255.0f,
        (float)light.color.b/255.0f,
        (float)light.color.a/255.0f
    };
    SetShaderValue(shader, light.colorLoc, color, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, light.intensityLoc, &light.intensity, SHADER_UNIFORM_FLOAT);
}

static Light CreateLight(int type, Vector3 position, Vector3 target, Color color, float intensity, Shader shader)
{
    Light light = { 0 };

    if (lightCount < MAX_LIGHTS)
    {
        light.enabled = 1;
        light.type = type;
        light.position = position;
        light.target = target;
        light.color = color;
        light.intensity = intensity;
        light.active = true;
        
        light.enabledLoc = GetShaderLocation(shader, TextFormat("lights[%i].enabled", lightCount));
        light.typeLoc = GetShaderLocation(shader, TextFormat("lights[%i].type", lightCount));
        light.positionLoc = GetShaderLocation(shader, TextFormat("lights[%i].position", lightCount));
        light.targetLoc = GetShaderLocation(shader, TextFormat("lights[%i].target", lightCount));
        light.colorLoc = GetShaderLocation(shader, TextFormat("lights[%i].color", lightCount));
        light.intensityLoc = GetShaderLocation(shader, TextFormat("lights[%i].intensity", lightCount));
        
        UpdateLight(shader, light);

        lightCount++;
    }

    return light;
}

const int W = 1500;
const int H = 950;

const Color bgColor = {0, 0, 0, 0};

double viewBobTime = 0.0f;

void controls(Camera3D *camera, float speed, float rotSpeed);
Vector3 applyViewBob(double dt);
bool isMoving = false;

Camera3D camera = {0};
Shader shader = {0};

int main()
{
    InitWindow(W, H, "Horror game");

    camera.position = (Vector3){3.12f, 2.0f, -14.2f};
    camera.target = (Vector3){-5.2f, 3.0f, 0.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Vector3 cubePosition = {0.0f, 1.0f, 0.0f};

    // load basic lighting shader
    shader = LoadShader(TextFormat("resources/shaders/glsl%i/lighting.vs", GLSL_VERSION),
                        TextFormat("resources/shaders/glsl%i/lighting.fs", GLSL_VERSION));

    // get some required shader locations
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MAP_ALBEDO] = GetShaderLocation(shader, "albedoMap");
    shader.locs[SHADER_LOC_MAP_METALNESS] = GetShaderLocation(shader, "mraMap");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "normalMap");
    shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(shader, "emissiveMap");
    shader.locs[SHADER_LOC_COLOR_DIFFUSE] = GetShaderLocation(shader, "albedoColor");

    int lightCountLoc = GetShaderLocation(shader, "numOfLights");
    int maxLightCount = MAX_LIGHTS;
    SetShaderValue(shader, lightCountLoc, &maxLightCount, SHADER_UNIFORM_INT);

    int usage = 0;
    SetShaderValue(shader, GetShaderLocation(shader, "useTexAlbedo"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "useTexNormal"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "useTexEmissive"), &usage, SHADER_UNIFORM_INT);

    int textureTilingLoc = GetShaderLocation(shader, "tiling");
    Vector2 textureTiling = (Vector2){ 1.0f, 1.0f };
    SetShaderValue(shader, textureTilingLoc, &textureTiling, SHADER_UNIFORM_VEC2);

    int ambientLoc = GetShaderLocation(shader, "ambient");
    int ambientColorLoc = GetShaderLocation(shader, "ambientColor");
    SetShaderValue(shader, ambientLoc, (float[4]){0.1f, 0.1f, 0.1f, 1.0f}, SHADER_UNIFORM_VEC4);

    // setup ambient color and intesity parameters
    float ambientIntensity = 0.005f;
    Color ambientColor = (Color){15, 20, 60, 255};
    Vector3 ambientColorNormalized = (Vector3){ambientColor.r / 255.0f, ambientColor.g / 255.0f, ambientColor.b / 255.0f};

    SetShaderValue(shader, ambientColorLoc, &ambientColorNormalized, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, ambientLoc, &ambientIntensity, SHADER_UNIFORM_FLOAT);

    int metallicValueLoc = GetShaderLocation(shader, "metallicValue");
    int roughnessValueLoc = GetShaderLocation(shader, "roughnessValue");
    int emissiveIntensityLoc = GetShaderLocation(shader, "emissivePower");
    int emissiveColorLoc = GetShaderLocation(shader, "emissiveColor");
    int aoValueLoc = GetShaderLocation(shader, "aoValue");

    float aoPower = 1.0f;
    SetShaderValue(shader, aoValueLoc, &aoPower, SHADER_UNIFORM_FLOAT);

    Model model = LoadModel("assets/dungeon+assets.glb");
    BoundingBox bounds = GetMeshBoundingBox(model.meshes[0]);

    for (int i = 0; i < model.materialCount; i++)
    {
        model.materials[i].shader = shader;
        model.materials[i].maps[MATERIAL_MAP_METALNESS].value = 0.0f;
        model.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value = 0.8f;
        model.materials[i].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
    }

    Light lights[MAX_LIGHTS] = {0};
    lights[0] = CreateLight(LIGHT_POINT, (Vector3){-2, 4, -2}, Vector3Zero(), RED, 10.0f, shader);
    lights[1] = CreateLight(LIGHT_POINT, (Vector3){-2, 4, -10}, Vector3Zero(), YELLOW, 10.0f, shader);
    lights[2] = CreateLight(LIGHT_POINT, (Vector3){-12, 4, -2}, Vector3Zero(), BLUE, 10.0f, shader);

    int cameraMode = CAMERA_FIRST_PERSON;

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, cameraMode);

        // update the shader with the camera view vector (points towards { 0.0f, 0.0f, 0.0f })
        float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z}; // array
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        lights[0].enabled = true;
        lights[1].enabled = true;
        lights[2].enabled = true;

        // Update light values on shader
        for (int i = 0; i < MAX_LIGHTS; i++) UpdateLight(shader, lights[i]);

        BeginDrawing();
        ClearBackground(bgColor);

        BeginMode3D(camera);

        BeginShaderMode(shader);

        // Set model and emissive color parameters on shader
        Vector4 modelEmissiveColor = ColorNormalize(model.materials[0].maps[MATERIAL_MAP_EMISSION].color);
        SetShaderValue(shader, emissiveColorLoc, &modelEmissiveColor, SHADER_UNIFORM_VEC4);

        float emissivePower = 1.0f;
        SetShaderValue(shader, emissiveIntensityLoc, &emissivePower, SHADER_UNIFORM_FLOAT);

        // set metallic and roughness values
        SetShaderValue(shader, metallicValueLoc, &model.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, roughnessValueLoc, &model.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

        DrawModel(model, cubePosition, 1.0F, WHITE);

        EndShaderMode();

        // Draw Lights
        for (int i = 0; i < MAX_LIGHTS; i++)
        {
            DrawSphereEx(lights[i].position, 0.2f, 8, 8, lights[i].color);
        }

        EndMode3D();
        DrawText(TextFormat("cam-x: %f, cam-y: %f, cam-z: %f", camera.position.x, camera.position.y, camera.position.z), 10, 40, 20, DARKGRAY);
        DrawText(TextFormat("targ-x: %f, targ-y: %f, targ-z: %f", camera.target.x, camera.target.y, camera.target.z), 10, 65, 20, DARKGRAY);

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