// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/ui/views/title_bar.h"

#include "chrome/browser/themes/theme_properties.h"
#include "electron/grit/electron_resources.h"
#include "shell/browser/ui/views/win_caption_button.h"
#include "shell/common/keyboard_util.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/icon_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace electron {

namespace {

gfx::ImageSkia GetWindowIcon() {
  HICON icon_handle = ::LoadIcon(NULL, IDI_APPLICATION);

  if (!icon_handle)
    return gfx::ImageSkia();

  const SkBitmap icon_bitmap = IconUtil::CreateSkBitmapFromHICON(icon_handle);

  if (icon_bitmap.isNull())
    return gfx::ImageSkia();

  return gfx::ImageSkia::CreateFrom1xBitmap(icon_bitmap);
}

}  // namespace

const char IconView::kViewClassName[] = "ElectronTitleBarIconView";

IconView::IconView(TitleBar* title_bar)
    : views::Button(title_bar), title_bar_(title_bar) {
  SetFocusBehavior(View::FocusBehavior::NEVER);
}

gfx::Size IconView::CalculatePreferredSize() const {
  return gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize);
}

const char* IconView::GetClassName() const {
  return kViewClassName;
}

void IconView::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ImageSkia icon = GetWindowIcon();
  if (!icon.isNull()) {
    PaintIcon(canvas, icon);
  }
}

void IconView::PaintIcon(gfx::Canvas* canvas, const gfx::ImageSkia& image) {
  // For source images smaller than the favicon square, scale them as if they
  // were padded to fit the favicon square, so we don't blow up tiny favicons
  // into larger or nonproportional results.
  float float_src_w = static_cast<float>(image.width());
  float float_src_h = static_cast<float>(image.height());
  float scalable_w, scalable_h;
  if (image.width() <= gfx::kFaviconSize &&
      image.height() <= gfx::kFaviconSize) {
    scalable_w = scalable_h = gfx::kFaviconSize;
  } else {
    scalable_w = float_src_w;
    scalable_h = float_src_h;
  }

  // Scale proportionately.
  float scale = std::min(static_cast<float>(width()) / scalable_w,
                         static_cast<float>(height()) / scalable_h);
  int dest_w = static_cast<int>(float_src_w * scale);
  int dest_h = static_cast<int>(float_src_h * scale);

  // Center the scaled image.
  canvas->DrawImageInt(image, 0, 0, image.width(), image.height(),
                       (width() - dest_w) / 2, (height() - dest_h) / 2, dest_w,
                       dest_h, true);
}

const views::Widget* IconView::widget() const {
  return title_bar_->GetWidget();
}

const char TitleBar::kViewClassName[] = "ElectronTitleBar";

TitleBar::TitleBar() {
  UpdateViewColors();
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));

  hamburger_button_ =
      CreateCaptionButton(VIEW_ID_HAMBURGER_BUTTON, IDS_APP_ACCNAME_HAMBURGER);

  window_icon_ = new IconView(this);
  AddChildView(window_icon_);

  window_title_ = new views::Label(base::string16());
  window_title_->SetSubpixelRenderingEnabled(false);
  window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  window_title_->SetID(VIEW_ID_WINDOW_TITLE);
  AddChildView(window_title_);

  minimize_button_ =
      CreateCaptionButton(VIEW_ID_MINIMIZE_BUTTON, IDS_APP_ACCNAME_MINIMIZE);
  maximize_button_ =
      CreateCaptionButton(VIEW_ID_MAXIMIZE_BUTTON, IDS_APP_ACCNAME_MAXIMIZE);
  restore_button_ =
      CreateCaptionButton(VIEW_ID_RESTORE_BUTTON, IDS_APP_ACCNAME_RESTORE);
  close_button_ =
      CreateCaptionButton(VIEW_ID_CLOSE_BUTTON, IDS_APP_ACCNAME_CLOSE);
}

TitleBar::~TitleBar() {}

int TitleBar::Height() const {
  return display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYCAPTION) +
         display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSIZEFRAME) +
         (GetWidget() && GetWidget()->IsMaximized()
              ? 0
              : display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYEDGE));
}

SkColor TitleBar::GetFrameColor() const {
  ThemeProperties::OverwritableByUserThemeProperty color_id;
  color_id = (GetWidget() && GetWidget()->ShouldPaintAsActive())
                 ? ThemeProperties::COLOR_FRAME
                 : ThemeProperties::COLOR_FRAME_INACTIVE;

  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  return theme_provider ? theme_provider->GetColor(color_id) : SK_ColorWHITE;
}

int TitleBar::NonClientHitTest(const gfx::Point& point) {
  if (bounds().Contains(point)) {
    switch (GetEventHandlerForPoint(point)->GetID()) {
      case VIEW_ID_MINIMIZE_BUTTON:
      case VIEW_ID_MAXIMIZE_BUTTON:
      case VIEW_ID_RESTORE_BUTTON:
      case VIEW_ID_CLOSE_BUTTON: {
        return HTCLIENT;
      }
      default:
        return HTCAPTION;
    }
  }

  return HTNOWHERE;
}

const char* TitleBar::GetClassName() const {
  return kViewClassName;
}

void TitleBar::AddedToWidget() {
  Layout();
  GetWidget()->RemoveObserver(this);
  GetWidget()->AddObserver(this);
}

void TitleBar::OnWidgetActivationChanged(views::Widget* widget, bool active) {
  UpdateViewColors();
  Layout();
}

void TitleBar::Layout() {
  if (GetWidget()) {
    LayoutCaptionButtons();
    LayoutTitleBar();
  }
}

void TitleBar::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == minimize_button_) {
    GetWidget()->Minimize();
  } else if (sender == maximize_button_) {
    GetWidget()->Maximize();
  } else if (sender == restore_button_) {
    GetWidget()->Restore();
  } else if (sender == close_button_) {
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
  }
}

WindowsCaptionButton* TitleBar::CreateCaptionButton(
    ViewID button_type,
    int accessible_name_resource_id) {
  WindowsCaptionButton* button = new WindowsCaptionButton(
      this, button_type,
      l10n_util::GetStringUTF16(accessible_name_resource_id));
  AddChildView(button);
  return button;
}

void TitleBar::LayoutTitleBar() {
  if (!ShowIcon() && !ShowTitle())
    return;

  gfx::Rect window_icon_bounds;
  const int icon_size =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSMICON);
  int next_leading_x =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
  constexpr int kMaximizedLeftMargin = 2;
  if (GetWidget()->IsMaximized())
    next_leading_x += kMaximizedLeftMargin;
  int next_trailing_x = minimize_button_->x();

  const int y = (Height() - icon_size) / 2;
  window_icon_bounds = gfx::Rect(next_leading_x, y, icon_size, icon_size);

  constexpr int kIconTitleSpacing = 5;
  if (ShowIcon()) {
    window_icon_->SetBoundsRect(window_icon_bounds);
    next_leading_x = window_icon_bounds.right() + kIconTitleSpacing;
  } else if (ShowHamburgerMenu()) {
    gfx::Size button_size = hamburger_button_->GetPreferredSize();
    hamburger_button_->SetBounds(0, 0, button_size.width(),
                                 button_size.height());
    next_leading_x = hamburger_button_->bounds().right() + kIconTitleSpacing;
  }

  if (ShowTitle()) {
    if (!ShowIcon() && !ShowHamburgerMenu()) {
      // This matches native Windows 10 UWP apps that don't have window icons.
      constexpr int kMinimumTitleLeftBorderMargin = 11;
      DCHECK_LE(next_leading_x, kMinimumTitleLeftBorderMargin);
      next_leading_x = kMinimumTitleLeftBorderMargin;
    }
    window_title_->SetText(GetWidget()->widget_delegate()->GetWindowTitle());
    const int max_text_width = std::max(0, next_trailing_x - next_leading_x);
    window_title_->SetBounds(next_leading_x, window_icon_bounds.y(),
                             max_text_width, window_icon_bounds.height());
    window_title_->SetAutoColorReadabilityEnabled(false);
  }
}

void TitleBar::LayoutCaptionButton(WindowsCaptionButton* button,
                                   int previous_button_x) {
  gfx::Size button_size = button->GetPreferredSize();
  button->SetBounds(previous_button_x - button_size.width(), 0,
                    button_size.width(), button_size.height());
}

void TitleBar::LayoutCaptionButtons() {
  LayoutCaptionButton(close_button_, width());

  LayoutCaptionButton(restore_button_, close_button_->x());
  restore_button_->SetVisible(GetWidget()->IsMaximized());

  LayoutCaptionButton(maximize_button_, close_button_->x());
  maximize_button_->SetVisible(!GetWidget()->IsMaximized());

  LayoutCaptionButton(minimize_button_, maximize_button_->x());
}

bool TitleBar::ShowTitle() const {
  return true;
}

bool TitleBar::ShowIcon() const {
  return false;
}

bool TitleBar::ShowHamburgerMenu() const {
  return true;
}

void TitleBar::UpdateWindowIcon() {
  Layout();
}

void TitleBar::UpdateWindowTitle() {
  Layout();
}

void TitleBar::UpdateViewColors() {
  SetBackground(views::CreateSolidBackground(GetFrameColor()));
}

void TitleBar::RequestSystemMenuAt(const gfx::Point& point) {
  LOG(INFO) << "RequestSystemMenuAt " << point.ToString() << '\n';
}

void TitleBar::RequestSystemMenu() {
  LOG(INFO) << "RequestSystemMenu" << '\n';
}

}  // namespace electron
