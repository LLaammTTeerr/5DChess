// Concrete state classes

// class PuzzlesState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "PUZZLES"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

// class GuideState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "GUIDE"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

// class SettingsMenuState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "SETTINGS_MENU"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

// class InGameState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "IN_GAME"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

// class PausedState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "PAUSED"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };
