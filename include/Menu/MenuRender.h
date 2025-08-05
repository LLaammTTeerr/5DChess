#pragma once

class IMenuRender {
public:
  virtual ~IMenuRender() = default;
  void render(const MenuComponent& menu);
  virtual void renderMenu(const Menu& menu) = 0;
  virtual void renderMenuItem(const MenuItem& item) = 0;
};


class RectangleMenuRender : public IMenuRender {
};

class ListMenuRender : public IMenuRender {
};

