#pragma once

enum class GameState
{
    UNKNOWN,
    FE,
    GAMEPLAY,
    NIS,
    LOADING
};

enum class LoadingContext
{
    NONE,
    FRONTEND,
    GAMEPLAY
};

enum class GameplaySubState
{
    NORMAL,
    PAUSED
};

namespace GameStateManager
{
    void Init();
    void Update();

    float GetGameDeltaTime();

    GameState GetState();
    GameplaySubState GetGameplaySubState();

    void SetState(GameState state);

    LoadingContext GetLoadingContext();

    void HandleMusicState();
}