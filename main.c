#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <raylib.h>
#include <math.h>
#include <stdlib.h>   
#include <raymath.h>

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else
    #define GLSL_VERSION            100
#endif


#define MAX_LIGHTS  4 

static int W = 1500;
static int H = 950;

double viewBobTime = 0.0f;

void controls(Camera3D *camera, float speed, float rotSpeed);
Vector3 applyViewBob(double dt);
bool isMoving = false;

typedef enum {
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT,
    LIGHT_SPOT
} LightType;

typedef struct {
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

    camera.position = (Vector3){ 2.0f, 2.0f, 6.0f };   
    camera.target = (Vector3){ 0.0f, 0.5f, 0.0f };    
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };         
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

    float ambientIntensity = 0.02f;
    Color ambientColor = (Color){ 26, 32, 135, 255 };
    Vector3 ambientColorNormalized = (Vector3){ ambientColor.r/255.0f, ambientColor.g/255.0f, ambientColor.b/255.0f };
    SetShaderValue(shader, GetShaderLocation(shader, "ambientColor"), &ambientColorNormalized, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "ambient"), &ambientIntensity, SHADER_UNIFORM_FLOAT);

    int metallicValueLoc = GetShaderLocation(shader, "metallicValue");
    int roughnessValueLoc = GetShaderLocation(shader, "roughnessValue");
    int emissiveIntensityLoc = GetShaderLocation(shader, "emissivePower");
    int emissiveColorLoc = GetShaderLocation(shader, "emissiveColor");
    int textureTilingLoc = GetShaderLocation(shader, "tiling");

    Model car = LoadModel("resources/models/old_car_new.glb");

    car.materials[0].shader = shader;

    car.materials[0].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    car.materials[0].maps[MATERIAL_MAP_METALNESS].value = 1.0f;
    car.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value = 0.0f;
    car.materials[0].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
    car.materials[0].maps[MATERIAL_MAP_EMISSION].color = (Color){ 255, 162, 0, 255 };

    car.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = LoadTexture("resources/old_car_d.png");
    car.materials[0].maps[MATERIAL_MAP_METALNESS].texture = LoadTexture("resources/old_car_mra.png");
    car.materials[0].maps[MATERIAL_MAP_NORMAL].texture = LoadTexture("resources/old_car_n.png");
    car.materials[0].maps[MATERIAL_MAP_EMISSION].texture = LoadTexture("resources/old_car_e.png");

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

    Vector2 carTextureTiling = (Vector2){ 0.5f, 0.5f };
    Vector2 floorTextureTiling = (Vector2){ 10.0f, 10.0f };

    Light lights[MAX_LIGHTS] = { 0 };
    lights[0] = CreateLight(LIGHT_POINT, (Vector3){ -2.0f, 2.0f, -3.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, YELLOW, 4.0f, shader);
    lights[1] = CreateLight(LIGHT_POINT, (Vector3){ 3.0f, 2.0f, 2.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, GREEN, 3.3f, shader);
    lights[2] = CreateLight(LIGHT_POINT, (Vector3){ -3.0f, 2.0f, 2.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, RED, 8.3f, shader);
    lights[3] = CreateLight(LIGHT_POINT, (Vector3){ 2.0f, 2.0f, -3.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, BLUE, 2.0f, shader);

    int usage = 1;
    SetShaderValue(shader, GetShaderLocation(shader, "useTexAlbedo"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "useTexNormal"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "useTexEmissive"), &usage, SHADER_UNIFORM_INT);

    SetTargetFPS(60);                  
    while (!WindowShouldClose())
    {

        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        for (int i = 0; i < MAX_LIGHTS; i++) UpdateLight(shader, lights[i]);

        BeginDrawing();

            ClearBackground(BLACK);

            BeginMode3D(camera);

             
                SetShaderValue(shader, textureTilingLoc, &floorTextureTiling, SHADER_UNIFORM_VEC2);
                Vector4 floorEmissiveColor = ColorNormalize(floor.materials[0].maps[MATERIAL_MAP_EMISSION].color);
                SetShaderValue(shader, emissiveColorLoc, &floorEmissiveColor, SHADER_UNIFORM_VEC4);

                SetShaderValue(shader, metallicValueLoc, &floor.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
                SetShaderValue(shader, roughnessValueLoc, &floor.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

                DrawModel(floor, (Vector3){ 0.0f, 0.0f, 0.0f }, 80.0f, WHITE);  


                SetShaderValue(shader, textureTilingLoc, &carTextureTiling, SHADER_UNIFORM_VEC2);
                Vector4 carEmissiveColor = ColorNormalize(car.materials[0].maps[MATERIAL_MAP_EMISSION].color);
                SetShaderValue(shader, emissiveColorLoc, &carEmissiveColor, SHADER_UNIFORM_VEC4);
                float emissiveIntensity = 0.01f;
                SetShaderValue(shader, emissiveIntensityLoc, &emissiveIntensity, SHADER_UNIFORM_FLOAT);

                SetShaderValue(shader, metallicValueLoc, &car.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
                SetShaderValue(shader, roughnessValueLoc, &car.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

                DrawModel(car, (Vector3){ 0.0f, 0.0f, 0.0f }, 0.45f, WHITE); 

                for (int i = 0; i < MAX_LIGHTS; i++)
                {
                    Color lightColor = (Color){
                        (unsigned char)(lights[i].color[0]*255),
                        (unsigned char)(lights[i].color[1]*255),
                        (unsigned char)(lights[i].color[2]*255),
                        (unsigned char)(lights[i].color[3]*255) };

                    if (lights[i].enabled) DrawSphereEx(lights[i].position, 0.2f, 8, 8, lightColor);
                    else DrawSphereWires(lights[i].position, 0.2f, 8, 8, ColorAlpha(lightColor, 0.3f));
                }

            EndMode3D();

            DrawText("Toggle lights: [1][2][3][4]", 10, 40, 20, LIGHTGRAY);

            DrawText("(c) Old Rusty Car model by Renafox (https://skfb.ly/LxRy)", screenWidth - 320, screenHeight - 20, 10, LIGHTGRAY);

            DrawFPS(10, 10);

            controls(&camera, 0.1f, 1.5f);

        EndDrawing();
    }

    car.materials[0].shader = (Shader){ 0 };
    UnloadMaterial(car.materials[0]);
    car.materials[0].maps = NULL;
    UnloadModel(car);

    floor.materials[0].shader = (Shader){ 0 };
    UnloadMaterial(floor.materials[0]);
    floor.materials[0].maps = NULL;
    UnloadModel(floor);

    UnloadShader(shader);       

    CloseWindow();             
  

    return 0;
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
        light.color[0] = (float)color.r/255.0f;
        light.color[1] = (float)color.g/255.0f;
        light.color[2] = (float)color.b/255.0f;
        light.color[3] = (float)color.a/255.0f;
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

    float position[3] = { light.position.x, light.position.y, light.position.z };
    SetShaderValue(shader, light.positionLoc, position, SHADER_UNIFORM_VEC3);


    float target[3] = { light.target.x, light.target.y, light.target.z };
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