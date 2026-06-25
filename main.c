#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <raylib.h>
#include <math.h>
#include <stdlib.h>
#include <raymath.h>

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION 330
#else
#define GLSL_VERSION 100
#endif

#define MAX_LIGHTS 4

static int W = 1500;
static int H = 950;

double viewBobTime = 0.0f;

void controls(Camera3D *camera, float speed, float rotSpeed);
Vector3 applyViewBob(double dt);
bool isMoving = false;

typedef enum
{
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT,
    LIGHT_SPOT
} LightType;

typedef struct
{
    int type;
    int enabled;
    Vector3 position;
    Vector3 target;
    float color[4];
    float intensity;

    int typeLoc;
    int enabledLoc;
    int positionLoc;
    int targetLoc;
    int colorLoc;
    int intensityLoc;
} Light;

static int lightCount = 0;

Camera3D camera = {0};

static Light CreateLight(int type, Vector3 position, Vector3 target, Color color, float intensity, Shader shader);

static void UpdateLight(Shader shader, Light light);

int main(void)
{

    const int screenWidth = W;
    const int screenHeight = H;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - basic pbr");

    camera.position = (Vector3){2.0f, 2.0f, 6.0f};
    camera.target = (Vector3){0.0f, 0.5f, 0.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/pbr.vs", GLSL_VERSION),
                               TextFormat("resources/shaders/glsl%i/pbr.fs", GLSL_VERSION));
    shader.locs[SHADER_LOC_MAP_ALBEDO] = GetShaderLocation(shader, "albedoMap");

    shader.locs[SHADER_LOC_MAP_METALNESS] = GetShaderLocation(shader, "mraMap");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "normalMap");

    shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(shader, "emissiveMap");
    shader.locs[SHADER_LOC_COLOR_DIFFUSE] = GetShaderLocation(shader, "albedoColor");

    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    int lightCountLoc = GetShaderLocation(shader, "numOfLights");
    int maxLightCount = MAX_LIGHTS;
    SetShaderValue(shader, lightCountLoc, &maxLightCount, SHADER_UNIFORM_INT);

    float ambientIntensity = 0.01f;
    Color ambientColor = (Color){26, 32, 20, 100};
    Vector3 ambientColorNormalized = (Vector3){ambientColor.r / 255.0f, ambientColor.g / 255.0f, ambientColor.b / 255.0f};
    SetShaderValue(shader, GetShaderLocation(shader, "ambientColor"), &ambientColorNormalized, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "ambient"), &ambientIntensity, SHADER_UNIFORM_FLOAT);

    int metallicValueLoc = GetShaderLocation(shader, "metallicValue");
    int roughnessValueLoc = GetShaderLocation(shader, "roughnessValue");
    int aoValueLoc = GetShaderLocation(shader, "aoValue");
    int emissiveIntensityLoc = GetShaderLocation(shader, "emissivePower");
    int emissiveColorLoc = GetShaderLocation(shader, "emissiveColor");
    int textureTilingLoc = GetShaderLocation(shader, "tiling");

    Model baseLamp = LoadModel("resources/models/base_lamp.glb");

    // Apply PBR shader to ALL baseLamp materials so every mesh reacts to lighting
    for (int i = 0; i < baseLamp.materialCount; i++)
    {
        baseLamp.materials[i].shader = shader;
        baseLamp.materials[i].maps[MATERIAL_MAP_ALBEDO].color = (Color){40, 40, 40, 255};
        baseLamp.materials[i].maps[MATERIAL_MAP_METALNESS].value = 0.0f;
        baseLamp.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value = 0.8f;
        baseLamp.materials[i].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
        baseLamp.materials[i].maps[MATERIAL_MAP_EMISSION].color = BLACK;
    }

    Model wall = LoadModel("resources/models/building_wall.glb");

    // Apply PBR shader to ALL wall materials so every mesh reacts to lighting
    for (int i = 0; i < wall.materialCount; i++)
    {
        wall.materials[i].shader = shader;
        // Albedo color = multiplier on the texture. WHITE = no change (too bright).
        // Tune this to adjust texture darkness/contrast. Lower = darker.
        wall.materials[i].maps[MATERIAL_MAP_ALBEDO].color = (Color){60, 50, 50, 255};
        wall.materials[i].maps[MATERIAL_MAP_METALNESS].value = 0.0f;
        wall.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value = 1.0f;
        wall.materials[i].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
        wall.materials[i].maps[MATERIAL_MAP_EMISSION].color = BLACK;

        wall.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = LoadTexture("resources/wall_d.jpg");
        wall.materials[i].maps[MATERIAL_MAP_NORMAL].texture = LoadTexture("resources/wall_n.jpg");
    }

    Model gate = LoadModel("resources/models/steel_gate.glb");

    Matrix rotation = MatrixRotateY(90.0f * DEG2RAD);
    gate.transform = rotation;

    // Apply PBR shader to ALL wall materials so every mesh reacts to lighting
    for (int i = 0; i < gate.materialCount; i++)
    {
        gate.materials[i].shader = shader;
        // Albedo color = multiplier on the texture. WHITE = no change (too bright).
        // Tune this to adjust texture darkness/contrast. Lower = darker.
        gate.materials[i].maps[MATERIAL_MAP_ALBEDO].color = (Color){60, 50, 50, 255};
        gate.materials[i].maps[MATERIAL_MAP_METALNESS].value = 0.7f;
        gate.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value = 1.0f;
        gate.materials[i].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
        gate.materials[i].maps[MATERIAL_MAP_EMISSION].color = BLACK;
    }

    Model car = LoadModel("resources/models/old_car_new.glb");

    car.materials[0].shader = shader;

    car.materials[0].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    car.materials[0].maps[MATERIAL_MAP_METALNESS].value = 1.0f;
    car.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value = 0.0f;
    car.materials[0].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
    car.materials[0].maps[MATERIAL_MAP_EMISSION].color = (Color){255, 162, 0, 255};

    car.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = LoadTexture("resources/old_car_d.png");
    car.materials[0].maps[MATERIAL_MAP_METALNESS].texture = LoadTexture("resources/old_car_mra.png");
    car.materials[0].maps[MATERIAL_MAP_NORMAL].texture = LoadTexture("resources/old_car_n.png");
    car.materials[0].maps[MATERIAL_MAP_EMISSION].texture = LoadTexture("resources/old_car_e.png");

    Model car2 = LoadModel("resources/models/old_rusty_car_2.glb");

    // Generate tangents for every mesh in the model so normal mapping works
    for (int i = 0; i < car2.meshCount; i++)
    {
        GenMeshTangents(&car2.meshes[i]);
    }

    Texture2D car2Albedo = LoadTexture("resources/old_car2_d.png");
    Texture2D car2MRA = LoadTexture("resources/old_car2_mra.png");
    Texture2D car2Normal = LoadTexture("resources/old_car2_n.png");

    for (int i = 0; i < car2.materialCount; i++)
    {
        car2.materials[i].shader = shader;

        car2.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
        car2.materials[i].maps[MATERIAL_MAP_METALNESS].value = 1.0f;
        car2.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value = 0.0f;
        car2.materials[i].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
        car2.materials[i].maps[MATERIAL_MAP_EMISSION].color = (Color){255, 162, 0, 255};

        // Assign pre-loaded textures
        car2.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = car2Albedo;
        car2.materials[i].maps[MATERIAL_MAP_METALNESS].texture = car2MRA;
        car2.materials[i].maps[MATERIAL_MAP_NORMAL].texture = car2Normal;
    }

    Model floor = LoadModel("resources/models/plane.glb");

    floor.materials[0].shader = shader;

    floor.materials[0].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    floor.materials[0].maps[MATERIAL_MAP_METALNESS].value = 0.8f;
    floor.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value = 0.1f;
    floor.materials[0].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
    floor.materials[0].maps[MATERIAL_MAP_EMISSION].color = BLACK;

    floor.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = LoadTexture("resources/road_a.png");
    floor.materials[0].maps[MATERIAL_MAP_METALNESS].texture = LoadTexture("resources/road_mra.png");
    floor.materials[0].maps[MATERIAL_MAP_NORMAL].texture = LoadTexture("resources/road_n.png");

    Vector2 carTextureTiling = (Vector2){0.5f, 0.5f};
    Vector2 car2TextureTiling = (Vector2){0.5f, 0.5f};
    Vector2 floorTextureTiling = (Vector2){10.0f, 10.0f};

    // 0.5 cancels the *2.0 in pbr.vs, giving the GLB's native 1:1 UV mapping
    Vector2 wallTextureTiling = (Vector2){0.5f, 0.5f};
    Vector2 gateTextureTiling = (Vector2){0.5f, 0.5f};
    Vector2 baseLampTextureTiling = (Vector2){1.0f, 1.0f};

    Light lights[MAX_LIGHTS] = {0};
    lights[0] = CreateLight(LIGHT_POINT, (Vector3){2.39f, 6.02f, -3.5f}, (Vector3){0.0f, 0.0f, 0.0f}, YELLOW, 4.0f, shader);
    lights[1] = CreateLight(LIGHT_POINT, (Vector3){3.0f, 2.0f, 2.0f}, (Vector3){0.0f, 0.0f, 0.0f}, GREEN, 3.3f, shader);
    lights[2] = CreateLight(LIGHT_POINT, (Vector3){-3.0f, 2.0f, 2.0f}, (Vector3){0.0f, 0.0f, 0.0f}, RED, 8.3f, shader);
    lights[3] = CreateLight(LIGHT_POINT, (Vector3){2.0f, 2.0f, -3.0f}, (Vector3){0.0f, 0.0f, 0.0f}, BLUE, 2.0f, shader);

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        for (int i = 0; i < MAX_LIGHTS; i++)
            UpdateLight(shader, lights[i]);

        BeginDrawing();

        ClearBackground(BLACK);

        BeginMode3D(camera);

        // baseLamp — uses textures embedded in the GLB; no external MRA/normal/emissive maps
        int useAlbedo = 1, noTex = 0;
        SetShaderValue(shader, GetShaderLocation(shader, "useTexAlbedo"), &useAlbedo, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexNormal"), &noTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &noTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexEmissive"), &noTex, SHADER_UNIFORM_INT);

        SetShaderValue(shader, textureTilingLoc, &baseLampTextureTiling, SHADER_UNIFORM_VEC2);
        Vector4 baseLampEmissiveColor = ColorNormalize(baseLamp.materials[0].maps[MATERIAL_MAP_EMISSION].color);
        SetShaderValue(shader, emissiveColorLoc, &baseLampEmissiveColor, SHADER_UNIFORM_VEC4);
        float baseLampEmissiveIntensity = 0.5f;
        SetShaderValue(shader, emissiveIntensityLoc, &baseLampEmissiveIntensity, SHADER_UNIFORM_FLOAT);

        float baseLampAo = 0.5f;
        SetShaderValue(shader, aoValueLoc, &baseLampAo, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, metallicValueLoc, &baseLamp.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, roughnessValueLoc, &baseLamp.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

        DrawModel(baseLamp, (Vector3){-4.0f, 0.0f, 0.0f}, 2.0f, (Color){40, 40, 40, 255});

        int useTex = 1;

        // wall — uses textures embedded in the GLB; no external MRA/normal/emissive maps

        SetShaderValue(shader, GetShaderLocation(shader, "useTexAlbedo"), &useTex, SHADER_UNIFORM_INT);  // 1
        SetShaderValue(shader, GetShaderLocation(shader, "useTexNormal"), &useTex, SHADER_UNIFORM_INT);  // 1
        SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &noTex, SHADER_UNIFORM_INT);      // 0 - use scalar
        SetShaderValue(shader, GetShaderLocation(shader, "useTexEmissive"), &noTex, SHADER_UNIFORM_INT); // 0

        SetShaderValue(shader, textureTilingLoc, &wallTextureTiling, SHADER_UNIFORM_VEC2);
        Vector4 wallEmissiveColor = ColorNormalize(wall.materials[0].maps[MATERIAL_MAP_EMISSION].color);
        SetShaderValue(shader, emissiveColorLoc, &wallEmissiveColor, SHADER_UNIFORM_VEC4);
        float wallEmissiveIntensity = 0.0f;
        SetShaderValue(shader, emissiveIntensityLoc, &wallEmissiveIntensity, SHADER_UNIFORM_FLOAT);

        float wallAo = 1.0f;
        SetShaderValue(shader, aoValueLoc, &wallAo, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, metallicValueLoc, &wall.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, roughnessValueLoc, &wall.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

        DrawModel(wall, (Vector3){-0.4f, -3.0f, -9.7f}, 0.8f, WHITE);

        // gate — uses textures embedded in the GLB; no external MRA/normal/emissive maps

        SetShaderValue(shader, GetShaderLocation(shader, "useTexAlbedo"), &useTex, SHADER_UNIFORM_INT);  // 1
        SetShaderValue(shader, GetShaderLocation(shader, "useTexNormal"), &useTex, SHADER_UNIFORM_INT);  // 1
        SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &noTex, SHADER_UNIFORM_INT);      // 0 - use scalar
        SetShaderValue(shader, GetShaderLocation(shader, "useTexEmissive"), &noTex, SHADER_UNIFORM_INT); // 0

        SetShaderValue(shader, textureTilingLoc, &gateTextureTiling, SHADER_UNIFORM_VEC2);
        Vector4 gateEmissiveColor = ColorNormalize(gate.materials[0].maps[MATERIAL_MAP_EMISSION].color);
        SetShaderValue(shader, emissiveColorLoc, &gateEmissiveColor, SHADER_UNIFORM_VEC4);
        float gateEmissiveIntensity = 0.0f;
        SetShaderValue(shader, emissiveIntensityLoc, &gateEmissiveIntensity, SHADER_UNIFORM_FLOAT);

        float gateAo = 1.0f;
        SetShaderValue(shader, aoValueLoc, &gateAo, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, metallicValueLoc, &gate.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, roughnessValueLoc, &gate.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

        DrawModel(gate, (Vector3){10.0f, 3.3f, -4.7f}, 2.0f, WHITE);

        // floor — has full external texture set

        SetShaderValue(shader, GetShaderLocation(shader, "useTexAlbedo"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexNormal"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexEmissive"), &noTex, SHADER_UNIFORM_INT);

        SetShaderValue(shader, textureTilingLoc, &floorTextureTiling, SHADER_UNIFORM_VEC2);
        Vector4 floorEmissiveColor = ColorNormalize(floor.materials[0].maps[MATERIAL_MAP_EMISSION].color);
        SetShaderValue(shader, emissiveColorLoc, &floorEmissiveColor, SHADER_UNIFORM_VEC4);
        float floorEmissiveIntensity = 0.0f;
        SetShaderValue(shader, emissiveIntensityLoc, &floorEmissiveIntensity, SHADER_UNIFORM_FLOAT);

        float floorAo = 1.0f;
        SetShaderValue(shader, aoValueLoc, &floorAo, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, metallicValueLoc, &floor.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, roughnessValueLoc, &floor.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

        DrawModel(floor, (Vector3){0.0f, 0.0f, 0.0f}, 80.0f, WHITE);

        // car — has full external texture set including emissive
        SetShaderValue(shader, GetShaderLocation(shader, "useTexAlbedo"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexNormal"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexEmissive"), &useTex, SHADER_UNIFORM_INT);

        SetShaderValue(shader, textureTilingLoc, &carTextureTiling, SHADER_UNIFORM_VEC2);
        Vector4 carEmissiveColor = ColorNormalize(car.materials[0].maps[MATERIAL_MAP_EMISSION].color);
        SetShaderValue(shader, emissiveColorLoc, &carEmissiveColor, SHADER_UNIFORM_VEC4);
        float emissiveIntensity = 0.01f;
        SetShaderValue(shader, emissiveIntensityLoc, &emissiveIntensity, SHADER_UNIFORM_FLOAT);

        float carAo = 1.0f;
        SetShaderValue(shader, aoValueLoc, &carAo, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, metallicValueLoc, &car.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, roughnessValueLoc, &car.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

        DrawModel(car, (Vector3){0.0f, 0.0f, 0.0f}, 0.45f, WHITE);

        // car 2
        SetShaderValue(shader, GetShaderLocation(shader, "useTexAlbedo"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexNormal"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &useTex, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "useTexEmissive"), &noTex, SHADER_UNIFORM_INT);

        SetShaderValue(shader, textureTilingLoc, &car2TextureTiling, SHADER_UNIFORM_VEC2);
        Vector4 car2EmissiveColor = ColorNormalize(car2.materials[0].maps[MATERIAL_MAP_EMISSION].color);
        SetShaderValue(shader, emissiveColorLoc, &car2EmissiveColor, SHADER_UNIFORM_VEC4);
        float car2EmissiveIntensity = 0.01f;
        SetShaderValue(shader, emissiveIntensityLoc, &car2EmissiveIntensity, SHADER_UNIFORM_FLOAT);

        float car2Ao = 1.0f;
        SetShaderValue(shader, aoValueLoc, &car2Ao, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, metallicValueLoc, &car2.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, roughnessValueLoc, &car2.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

        DrawModel(car2, (Vector3){-5.0f, 0.0f, 0.0f}, 0.03f, WHITE);

        for (int i = 0; i < MAX_LIGHTS; i++)
        {
            Color lightColor = (Color){
                (unsigned char)(lights[i].color[0] * 255),
                (unsigned char)(lights[i].color[1] * 255),
                (unsigned char)(lights[i].color[2] * 255),
                (unsigned char)(lights[i].color[3] * 255)};

            if (lights[i].enabled)
                DrawSphereEx(lights[i].position, 0.22f, 8, 8, lightColor);
            else
                DrawSphereWires(lights[i].position, 0.22f, 8, 8, ColorAlpha(lightColor, 0.3f));
        }

        EndMode3D();

        DrawText("Toggle lights: [1][2][3][4]", 10, 40, 20, LIGHTGRAY);

        DrawText("(c) Old Rusty Car model by Renafox (https://skfb.ly/LxRy)", screenWidth - 320, screenHeight - 20, 10, LIGHTGRAY);

        DrawFPS(10, 10);

        controls(&camera, 0.1f, 1.5f);

        EndDrawing();
    }

    car.materials[0].shader = (Shader){0};
    UnloadMaterial(car.materials[0]);
    car.materials[0].maps = NULL;
    UnloadModel(car);

    for (int i = 0; i < car2.materialCount; i++)
    {
        car2.materials[i].shader = (Shader){0};
        UnloadMaterial(car2.materials[i]);
        car2.materials[i].maps = NULL;
    }

    UnloadModel(car2);

    for (int i = 0; i < wall.materialCount; i++)
    {
        wall.materials[i].shader = (Shader){0};
        UnloadMaterial(wall.materials[i]);
        wall.materials[i].maps = NULL;
    }
    UnloadModel(wall);

    for (int i = 0; i < gate.materialCount; i++)
    {
        gate.materials[i].shader = (Shader){0};
        UnloadMaterial(gate.materials[i]);
        gate.materials[i].maps = NULL;
    }
    UnloadModel(gate);

    for (int i = 0; i < baseLamp.materialCount; i++)
    {
        baseLamp.materials[i].shader = (Shader){0};
        UnloadMaterial(baseLamp.materials[i]);
        baseLamp.materials[i].maps = NULL;
    }
    UnloadModel(baseLamp);

    floor.materials[0].shader = (Shader){0};
    UnloadMaterial(floor.materials[0]);
    floor.materials[0].maps = NULL;
    UnloadModel(floor);

    UnloadShader(shader);

    CloseWindow();

    return 0;
}

static Light CreateLight(int type, Vector3 position, Vector3 target, Color color, float intensity, Shader shader)
{
    Light light = {0};

    if (lightCount < MAX_LIGHTS)
    {
        light.enabled = 1;
        light.type = type;
        light.position = position;
        light.target = target;
        light.color[0] = (float)color.r / 255.0f;
        light.color[1] = (float)color.g / 255.0f;
        light.color[2] = (float)color.b / 255.0f;
        light.color[3] = (float)color.a / 255.0f;
        light.intensity = intensity;

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

static void UpdateLight(Shader shader, Light light)
{
    SetShaderValue(shader, light.enabledLoc, &light.enabled, SHADER_UNIFORM_INT);
    SetShaderValue(shader, light.typeLoc, &light.type, SHADER_UNIFORM_INT);

    float position[3] = {light.position.x, light.position.y, light.position.z};
    SetShaderValue(shader, light.positionLoc, position, SHADER_UNIFORM_VEC3);

    float target[3] = {light.target.x, light.target.y, light.target.z};
    SetShaderValue(shader, light.targetLoc, target, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, light.colorLoc, light.color, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, light.intensityLoc, &light.intensity, SHADER_UNIFORM_FLOAT);
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