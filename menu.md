┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   MenuController│    │  MenuComponent  │    │   IMenuView     │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│-_gameState      │    │+addItem()       │    │+draw()          │
│-_menuSystem     │◆───│+removeItem()    │    │+createItemViews()│
│-_currentMenuModel│   │+findItem()      │    │-_itemViews[]    │
│-_menuView       │◆───│+setCommand()    │    └─────────────────┘
│-_menuFactory    │    │+clone()         │            ▲
├─────────────────┤    └─────────────────┘            │
│+update()        │            ▲                      │
│+draw()          │            │                      │
│+handleInput()   │            │              ┌─────────────────┐
└─────────────────┘    ┌───────┴─────────┐    │ ButtonMenuView  │
                       │                 │    ├─────────────────┤
              ┌────────┴────────┐ ┌──────┴──────┐│+draw()     │
              │    MenuItem     │ │    Menu     ││+createItemViews()│
              ├─────────────────┤ ├─────────────┤└─────────────────┘
              │-_title          │ │-_children[] │
              │-_command        │ │             │
              └─────────────────┘ └─────────────┘

┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│CompositeMenuFactory│ │   ICommand      │    │ MenuItemView    │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│+createMenuSystem()│  │+execute()       │    │-position        │
│+createMenuForState()│ │+clone()        │    │-size            │
│+createMainMenu()│    └─────────────────┘    │-isHovered       │
│+createSettingsMenu()│        ▲               ├─────────────────┤
└─────────────────┘            │               │+draw()          │
                       ┌───────┴─────────┐    │+isPointInside() │
                       │ StartGameCommand│    └─────────────────┘
                       └─────────────────┘