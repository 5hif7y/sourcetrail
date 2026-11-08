#include "Catch2.hpp"

#include "QtActions.h"

#include <QKeyEvent>

// TODO (petermost) do also negative tests

constexpr Qt::KeyboardModifier Alt = Qt::AltModifier;
constexpr Qt::KeyboardModifier Ctrl = Qt::ControlModifier;
constexpr Qt::KeyboardModifier Shift = Qt::ShiftModifier;

// We deliberately don't use the constants from QtActions, so we can detect wrong defined constants.

static Action detectAction(const int key, const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
	const QKeyEvent keyEvent(QKeyEvent::KeyPress, key, modifiers);

	return QtActions::detectAction(&keyEvent);
}

TEST_CASE("actions detect miscellaneous")
{
	REQUIRE(detectAction(Qt::Key_unknown) == Action::Unknown);

	REQUIRE(detectAction(Qt::Key_Escape) == Action::Cancel);
	REQUIRE(detectAction(Qt::Key_F4, Ctrl) == Action::CloseTab);
	REQUIRE(detectAction(Qt::Key_C, Ctrl) == Action::CopySelection);
	REQUIRE(detectAction(Qt::Key_R, Ctrl | Alt) == Action::RefreshUI);
	REQUIRE(detectAction(Qt::Key_Tab) == Action::SwitchGraphCodeFocus);
}

TEST_CASE("actions detect activate")
{
	REQUIRE(detectAction(Qt::Key_E)      == Action::ActivateFocus);
	REQUIRE(detectAction(Qt::Key_Return) == Action::ActivateFocus);

	REQUIRE(detectAction(Qt::Key_E, Shift)      == Action::ExpandFocus);
	REQUIRE(detectAction(Qt::Key_Return, Shift) == Action::ExpandFocus);

	REQUIRE(detectAction(Qt::Key_E, Ctrl | Shift)      == Action::ActivateFocusInNewTab);
	REQUIRE(detectAction(Qt::Key_Return, Ctrl | Shift) == Action::ActivateFocusInNewTab);
}

TEST_CASE("actions detect history")
{
	REQUIRE(detectAction(Qt::Key_Backspace) == Action::UndoHistory);

	REQUIRE(detectAction(Qt::Key_Y)         == Action::UndoHistory);
	REQUIRE(detectAction(Qt::Key_Z)         == Action::UndoHistory);

	REQUIRE(detectAction(Qt::Key_Y, Shift) == Action::RedoHistory);
	REQUIRE(detectAction(Qt::Key_Z, Shift) == Action::RedoHistory);
}

TEST_CASE("actions detect zoom")
{
	REQUIRE(detectAction(Qt::Key_Up, Ctrl | Shift) == Action::ZoomIn);
	REQUIRE(detectAction(Qt::Key_Down, Ctrl | Shift) == Action::ZoomOut);

	REQUIRE(detectAction(Qt::Key_0) == Action::ResetZoom);
}

TEST_CASE("actions detect screen search")
{
	REQUIRE(detectAction(Qt::Key_D, Ctrl) == Action::SearchScreen);
	REQUIRE(detectAction(Qt::Key_Slash) == Action::SearchScreen);
	REQUIRE(detectAction(Qt::Key_Question) == Action::SearchScreen);
}

TEST_CASE("actions detect up")
{
	REQUIRE(detectAction(Qt::Key_Up) == Action::MoveFocusUp);
	REQUIRE(detectAction(Qt::Key_Up, Shift) == Action::MoveReferenceFocusUp);
	REQUIRE(detectAction(Qt::Key_Up, Ctrl) == Action::MoveViewUp);

	// Vim:

	REQUIRE(detectAction(Qt::Key_K) == Action::MoveFocusUp);
	REQUIRE(detectAction(Qt::Key_K, Shift) == Action::MoveReferenceFocusUp);

	// Game:

	REQUIRE(detectAction(Qt::Key_W) == Action::MoveFocusUp);
	REQUIRE(detectAction(Qt::Key_W, Shift) == Action::MoveReferenceFocusUp);
}

TEST_CASE("actions detect down")
{
	REQUIRE(detectAction(Qt::Key_Down) == Action::MoveFocusDown);
	REQUIRE(detectAction(Qt::Key_Down, Shift) == Action::MoveReferenceFocusDown);
	REQUIRE(detectAction(Qt::Key_Down, Ctrl) == Action::MoveViewDown);

	// Vim:

	REQUIRE(detectAction(Qt::Key_J) == Action::MoveFocusDown);
	REQUIRE(detectAction(Qt::Key_J, Shift) == Action::MoveReferenceFocusDown);

	// Game:

	REQUIRE(detectAction(Qt::Key_S) == Action::MoveFocusDown);
	REQUIRE(detectAction(Qt::Key_S, Shift) == Action::MoveReferenceFocusDown);
}

TEST_CASE("actions detect left")
{
	REQUIRE(detectAction(Qt::Key_Left) == Action::MoveFocusLeft);
	REQUIRE(detectAction(Qt::Key_Left, Shift) == Action::MoveReferenceFocusLeft);
	REQUIRE(detectAction(Qt::Key_Left, Ctrl) == Action::MoveViewLeft);

	// Vim:

	REQUIRE(detectAction(Qt::Key_H) == Action::MoveFocusLeft);
	REQUIRE(detectAction(Qt::Key_H, Shift) == Action::MoveReferenceFocusLeft);

	// Game:

	REQUIRE(detectAction(Qt::Key_A) == Action::MoveFocusLeft);
	REQUIRE(detectAction(Qt::Key_A, Shift) == Action::MoveReferenceFocusLeft);
}

TEST_CASE("actions detect right")
{
	REQUIRE(detectAction(Qt::Key_Right) == Action::MoveFocusRight);
	REQUIRE(detectAction(Qt::Key_Right, Shift) == Action::MoveReferenceFocusRight);
	REQUIRE(detectAction(Qt::Key_Right, Ctrl) == Action::MoveViewRight);

	// Vim:

	REQUIRE(detectAction(Qt::Key_L) == Action::MoveFocusRight);
	REQUIRE(detectAction(Qt::Key_L, Shift) == Action::MoveReferenceFocusRight);

	// Game:

	REQUIRE(detectAction(Qt::Key_D) == Action::MoveFocusRight);
	REQUIRE(detectAction(Qt::Key_D, Shift) == Action::MoveReferenceFocusRight);
}
