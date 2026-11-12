#include "PluginEditor.h"

namespace vs
{

using namespace juce;
namespace pal = palette;

static String u8s (const char* s) { return String::fromUTF8 (s); }
static Font uiFont (float h, bool bold = false, float tracking = 0.0f)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain).withKerningFactor (tracking));
}

static constexpr int factoryIdOffset = 1;
static constexpr int userIdOffset = 1000;

// ---------------------------------------------------------------------------
VsLookAndFeel::VsLookAndFeel()
{
#if JUCE_WINDOWS
    setDefaultSansSerifTypefaceName ("Segoe UI"); // system font on Windows 10/11: modern look, no licence to ship
#endif
    setColour (ResizableWindow::backgroundColourId, pal::bg);
    setColour (Slider::rotarySliderFillColourId, pal::rose);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxTextColourId, pal::text);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, pal::rose.withAlpha (0.4f));
    setColour (Label::textColourId, pal::text);
    setColour (TextEditor::backgroundColourId, pal::panelHi);
    setColour (TextEditor::textColourId, pal::text);
    setColour (TextEditor::focusedOutlineColourId, pal::rose);
    setColour (ComboBox::backgroundColourId, pal::panelHi);
    setColour (ComboBox::textColourId, pal::text);
    setColour (ComboBox::outlineColourId, pal::panelEdge);
    setColour (ComboBox::arrowColourId, pal::rose);
    setColour (PopupMenu::backgroundColourId, pal::panel);
    setColour (PopupMenu::textColourId, pal::text);
    setColour (PopupMenu::headerTextColourId, pal::violet);
    setColour (PopupMenu::highlightedBackgroundColourId, pal::violetDeep);
    setColour (PopupMenu::highlightedTextColourId, pal::roseSoft);
    setColour (TextButton::buttonColourId, pal::panelHi);
    setColour (TextButton::textColourOffId, pal::dim);
    setColour (TextButton::textColourOnId, pal::text);
}

Font VsLookAndFeel::getTextButtonFont (TextButton& b, int height)
{
    return uiFont ((float) jmin (14, height - 10), b.getToggleState(), 0.02f);
}

Font VsLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (13.5f); }

Label* VsLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.5f, true));
    l->setColour (Label::textColourId, pal::text);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    return l;
}

void VsLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = jmin (r.getHeight() * 0.5f, 14.0f);
    const bool on = b.getToggleState();
    if (on)
    {
        // active: violet -> rose gradient with a soft neon halo
        g.setColour (pal::rose.withAlpha (0.16f));
        g.fillRoundedRectangle (r.expanded (1.0f), corner + 1.0f);
        g.setGradientFill (ColourGradient (pal::violet.darker (0.35f), r.getX(), r.getY(), pal::rose.darker (0.25f), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (pal::roseSoft.withAlpha (highlighted ? 0.55f : 0.35f));
        g.drawRoundedRectangle (r, corner, 1.0f);
    }
    else
    {
        g.setGradientFill (ColourGradient (pal::panelHi.brighter (highlighted ? 0.12f : 0.04f), 0, r.getY(), pal::panelHi.darker (0.25f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (highlighted ? pal::violet.withAlpha (0.5f) : pal::panelEdge.brighter (0.15f));
        g.drawRoundedRectangle (r, corner, 1.0f);
    }
    if (down)
    {
        g.setColour (Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (r, corner);
    }
}

void VsLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool highlighted, bool)
{
    const bool on = b.getToggleState();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (on ? pal::text : (highlighted ? pal::text : pal::dim.brighter (0.15f)));
    if (! b.isEnabled())
        g.setColour (pal::dim.withAlpha (0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), Justification::centred, 1, 0.85f);
}

void VsLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    const float corner = 8.0f;
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (box.isMouseOver (true) ? pal::violet.withAlpha (0.5f) : pal::panelEdge.brighter (0.15f));
    g.drawRoundedRectangle (r, corner, 1.0f);
    // arrow
    const float ax = r.getRight() - 18.0f, ay = r.getCentreY();
    Path p;
    p.addTriangle (ax - 5.0f, ay - 2.5f, ax + 5.0f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (pal::rose);
    g.fillPath (p);
}

void VsLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, Slider& s)
{
    const bool big = (bool) s.getProperties().getWithDefault ("vsBig", false);
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float diameter = jmin (bounds.getWidth(), bounds.getHeight());
    const auto area = bounds.withSizeKeepingCentre (diameter, diameter).reduced (diameter * (big ? 0.11f : 0.07f));
    const auto centre = area.getCentre();
    const float radius = area.getWidth() * 0.5f;
    const float ring = jmax (3.0f, radius * (big ? 0.075f : 0.10f));
    const auto colour = s.findColour (Slider::rotarySliderFillColourId);
    const bool enabled = s.isEnabled();
    const float arcR = radius - ring * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    auto pointOn = [&] (float r, float a) { return centre.getPointOnCircumference (r, a); };

    // soft neon halo behind the big knobs
    if (big && enabled)
    {
        const float haloR = radius * 1.12f;
        g.setGradientFill (ColourGradient (colour.withAlpha (0.20f + 0.10f * pos), centre.x, centre.y, colour.withAlpha (0.0f), centre.x + haloR, centre.y, true));
        g.fillEllipse (centre.x - haloR, centre.y - haloR, haloR * 2.0f, haloR * 2.0f);
    }

    // scale ticks around the big knobs
    if (big)
    {
        for (int i = 0; i <= 24; ++i)
        {
            const float a = startAngle + (float) i / 24.0f * (endAngle - startAngle);
            const bool major = i % 6 == 0;
            const float r0 = radius + ring * 0.6f, r1 = radius + ring * (major ? 1.7f : 1.15f);
            g.setColour ((a <= angle + 0.001f && enabled ? colour : pal::dim).withAlpha (major ? 0.8f : 0.4f));
            g.drawLine (Line<float> (pointOn (r0, a), pointOn (r1, a)), major ? 1.4f : 1.0f);
        }
    }

    // track + value arc (bipolar parameters grow from zero, e.g. pitch, formants, pan)
    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (pal::groove);
    g.strokePath (track, PathStrokeType (ring, PathStrokeType::curved, PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
    const float a0 = jmin (zeroAngle, angle), a1 = jmax (zeroAngle, angle);
    if (a1 - a0 > 0.001f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a1, true);
        const auto c0 = enabled ? pal::violet : pal::dim.withAlpha (0.3f);
        const auto c1 = enabled ? colour : pal::dim.withAlpha (0.3f);
        const auto p0 = pointOn (arcR, startAngle), p1 = pointOn (arcR, endAngle);
        if (enabled)
        {
            g.setGradientFill (ColourGradient (c0.withAlpha (0.22f), p0.x, p0.y, c1.withAlpha (0.30f), p1.x, p1.y, false));
            g.strokePath (value, PathStrokeType (ring * (s.isMouseOverOrDragging() ? 2.8f : 2.2f), PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setGradientFill (ColourGradient (c0, p0.x, p0.y, c1, p1.x, p1.y, false));
        g.strokePath (value, PathStrokeType (ring, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // knob body: drop shadow, dark metal radial gradient, lit rim
    const float bodyR = radius - ring * 2.0f;
    auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.translated (0.0f, bodyR * 0.06f).expanded (bodyR * 0.04f));
    g.setGradientFill (ColourGradient (Colour (0xff3b3150), centre.x - bodyR * 0.45f, centre.y - bodyR * 0.55f,
                                       Colour (0xff0d0a15), centre.x + bodyR * 0.55f, centre.y + bodyR * 0.85f, true));
    g.fillEllipse (body);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), centre.x, body.getY(), Colours::black.withAlpha (0.5f), centre.x, body.getBottom(), false));
    g.drawEllipse (body.reduced (0.6f), 1.2f);

    if (big)
    {
        // recessed display disc with the value
        const float discR = bodyR * 0.66f;
        auto disc = Rectangle<float> (discR * 2.0f, discR * 2.0f).withCentre (centre);
        g.setGradientFill (ColourGradient (Colour (0xff0b0912), centre.x, disc.getY(), Colour (0xff1a1426), centre.x, disc.getBottom(), false));
        g.fillEllipse (disc);
        g.setColour (colour.withAlpha (enabled ? 0.35f : 0.12f));
        g.drawEllipse (disc, 1.0f);
        g.setColour (enabled ? pal::text : pal::dim);
        g.setFont (uiFont (jlimit (11.0f, 26.0f, discR * 0.42f), true));
        g.drawFittedText (s.getTextFromValue (s.getValue()), disc.reduced (discR * 0.12f, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
    }

    // pointer: a lit dot on the body edge
    const float dotR = jmax (2.0f, ring * (big ? 0.55f : 0.45f));
    const auto dot = pointOn (bodyR - dotR * 2.2f, angle);
    if (enabled)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (dotR * 4.0f, dotR * 4.0f).withCentre (dot));
    }
    g.setColour (enabled ? pal::roseSoft : pal::dim);
    g.fillEllipse (Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (dot));
}

void VsLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto colour = s.findColour (Slider::rotarySliderFillColourId);
    const float cx = (float) x + (float) w * 0.38f;
    const float grooveW = 6.0f;
    const float top = (float) y + 8.0f, bottom = (float) (y + h) - 8.0f;
    auto groove = Rectangle<float> (cx - grooveW * 0.5f, top, grooveW, bottom - top);

    // dB scale: ticks drawn at fixed dB values converted through the slider's own mapping
    g.setFont (uiFont (10.5f));
    for (double db : { 6.0, 0.0, -12.0, -24.0, -36.0, -60.0 })
    {
        const float p = (float) s.valueToProportionOfLength (db);
        const float ty = bottom - p * (bottom - top);
        const bool major = db == 0.0 || db == -12.0 || db == -60.0;
        g.setColour (db == -12.0 ? colour.withAlpha (0.9f) : pal::dim.withAlpha (major ? 0.9f : 0.55f));
        g.drawLine (cx + grooveW * 0.5f + 12.0f, ty, cx + grooveW * 0.5f + (major ? 20.0f : 16.0f), ty, 1.0f);
        const String label = db <= -60.0 ? String::fromUTF8 ("−∞") : (db > 0 ? "+" : (db < 0 ? String::fromUTF8 ("−") : String())) + String ((int) std::abs (db));
        g.drawText (label, Rectangle<float> (cx + grooveW * 0.5f + 22.0f, ty - 7.0f, 30.0f, 14.0f), Justification::centredLeft);
    }

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (groove.expanded (1.5f), grooveW);
    g.setColour (pal::groove);
    g.fillRoundedRectangle (groove, grooveW * 0.5f);
    auto filled = groove.withTop (pos);
    const bool hot = s.getValue() > 0.0;
    g.setGradientFill (ColourGradient (hot ? pal::warn : colour, 0, filled.getY(), pal::violet.darker (0.2f), 0, filled.getBottom(), false));
    g.fillRoundedRectangle (filled, grooveW * 0.5f);
    g.setColour ((hot ? pal::warn : colour).withAlpha (0.18f));
    g.fillRoundedRectangle (filled.expanded (3.0f, 0.0f), grooveW);

    // hardware-style handle
    auto handle = Rectangle<float> (cx - 18.0f, pos - 10.0f, 36.0f, 20.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (handle.translated (0.0f, 2.0f), 5.0f);
    g.setGradientFill (ColourGradient (Colour (0xff3b3150), 0, handle.getY(), Colour (0xff120e1b), 0, handle.getBottom(), false));
    g.fillRoundedRectangle (handle, 5.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (handle.reduced (0.5f), 5.0f, 1.0f);
    const auto line = handle.withSizeKeepingCentre (24.0f, 2.5f);
    g.setColour ((hot ? pal::warn : colour).withAlpha (0.4f));
    g.fillRoundedRectangle (line.expanded (2.0f), 2.5f);
    g.setColour (hot ? pal::warn : pal::roseSoft);
    g.fillRoundedRectangle (line, 1.2f);
}

void VsLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto sw = r.removeFromLeft (38.0f).withSizeKeepingCentre (32.0f, 16.0f);
    const bool on = b.getToggleState();
    const bool enabled = b.isEnabled();
    if (on && enabled)
    {
        g.setColour (pal::rose.withAlpha (0.22f));
        g.fillRoundedRectangle (sw.expanded (2.5f), 10.0f);
        g.setGradientFill (ColourGradient (pal::violet, sw.getX(), 0, pal::rose, sw.getRight(), 0, false));
    }
    else
        g.setColour (on ? pal::dim.withAlpha (0.35f) : pal::groove);
    g.fillRoundedRectangle (sw, 8.0f);
    g.setColour (pal::panelEdge.brighter (0.2f));
    if (! on) g.drawRoundedRectangle (sw, 8.0f, 1.0f);
    g.setColour (enabled ? pal::text : pal::dim);
    g.fillEllipse (sw.getX() + (on ? sw.getWidth() - 14.0f : 2.0f), sw.getY() + 2.0f, 12.0f, 12.0f);
    g.setColour (! enabled ? pal::dim.withAlpha (0.45f) : (highlighted || on ? pal::text : pal::dim.brighter (0.2f)));
    g.setFont (uiFont (12.5f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), Justification::centredLeft, 2);
}

// ---------------------------------------------------------------------------
void MeterBar::setLevel (float linearPeak)
{
    const float db = linearPeak > 0.0f ? jmax (-60.0f, Decibels::gainToDecibels (linearPeak)) : -60.0f;
    const float next = db > displayDb ? db : jmax (db, displayDb - 0.7f);
    if (std::abs (next - displayDb) > 0.05f)
    {
        displayDb = next;
        repaint();
    }
}

void MeterBar::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float norm = jlimit (0.0f, 1.0f, (displayDb + 60.0f) / 60.0f);
    auto segColour = [] (float segDb) { return segDb > -3.0f ? pal::warn : (segDb > -18.0f ? pal::rose : pal::violet); };
    g.setFont (uiFont (10.5f, true, 0.06f));

    if (horizontal)
    {
        auto lab = r.removeFromLeft (30.0f);
        g.setColour (pal::dim);
        g.drawText (label, lab, Justification::centredLeft);
        auto led = r.removeFromRight (12.0f).reduced (0.0f, 4.0f);
        g.setColour (clipped ? pal::red : pal::groove);
        g.fillRoundedRectangle (led, 2.0f);
        r.removeFromRight (4.0f);
        auto bar = r.reduced (0.0f, 4.0f);
        const int n = jmax (8, (int) (bar.getWidth() / 4.0f));
        const float sw = bar.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float p = ((float) i + 0.5f) / (float) n;
            g.setColour (p <= norm ? segColour (-60.0f + 60.0f * p) : pal::groove);
            g.fillRect (Rectangle<float> (bar.getX() + (float) i * sw, bar.getY(), sw - 1.0f, bar.getHeight()));
        }
        return;
    }

    auto labelArea = r.removeFromBottom (16.0f);
    g.setColour (pal::dim);
    g.drawText (label, labelArea, Justification::centred);
    r.removeFromBottom (4.0f);
    auto bar = r.withSizeKeepingCentre (jmin (r.getWidth(), 12.0f), r.getHeight());
    auto led = bar.removeFromTop (7.0f);
    if (clipped)
    {
        g.setColour (pal::red.withAlpha (0.35f));
        g.fillRoundedRectangle (led.expanded (2.0f), 3.0f);
    }
    g.setColour (clipped ? pal::red : pal::groove);
    g.fillRoundedRectangle (led, 2.0f);
    bar.removeFromTop (3.0f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (bar.expanded (2.0f), 3.0f);
    const int n = jmax (8, (int) (bar.getHeight() / 4.0f));
    const float sh = bar.getHeight() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float p = ((float) i + 0.5f) / (float) n;
        const bool lit = p <= norm;
        auto seg = Rectangle<float> (bar.getX(), bar.getBottom() - (float) (i + 1) * sh, bar.getWidth(), sh - 1.0f);
        g.setColour (lit ? segColour (-60.0f + 60.0f * p) : pal::groove);
        g.fillRect (seg);
    }
}

// ---------------------------------------------------------------------------
void IdentityPanel::setLevel (float linearPeak)
{
    const float db = linearPeak > 0.0f ? Decibels::gainToDecibels (linearPeak) : -100.0f;
    const float target = jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    const float next = target > level ? level + 0.5f * (target - level) : level * 0.9f;
    if (std::abs (next - level) > 0.003f)
    {
        level = next;
        repaint();
    }
}

void IdentityPanel::resized()
{
    cache = {};
    buildSinger();
}

Point<float> IdentityPanel::u (float x, float y) const
{
    return Point<float> (x, y).transformedBy (toPanel);
}

Point<float> IdentityPanel::ringCentre() const
{
    return u (560.0f, 400.0f); // halo behind the head
}

// Builds every path of the singer once per size change (never per frame).
void IdentityPanel::buildSinger()
{
    const float w = (float) getWidth(), h = (float) getHeight();
    if (w <= 0.0f || h <= 0.0f)
        return;
    // fit ~620 design units across the panel (hair back to the sound waves), head near the top
    const float s = jmin (w / 620.0f, h / 1150.0f);
    toPanel = AffineTransform::translation (-520.0f, -60.0f).scaled (s).translated (w * 0.5f, h * 0.04f);

    Singer g;
    g.unit = s;
    auto pathFrom = [this] (std::initializer_list<float> v, bool close) {
        // v = x0,y0 then groups of 6 (cubic control points)
        Path p;
        auto it = v.begin();
        const float x0 = *it++, y0 = *it++;
        p.startNewSubPath (u (x0, y0));
        while (it != v.end())
        {
            const float a = *it++, b = *it++, c = *it++, d = *it++, e = *it++, f = *it++;
            p.cubicTo (u (a, b), u (c, d), u (e, f));
        }
        if (close)
            p.closeSubPath();
        return p;
    };

    // Front contour, chin raised as when singing: forehead, nose, open mouth, chin, throat, shoulder.
    const std::initializer_list<float> front {
        560, 170,
        630, 180, 668, 240, 675, 300,   // forehead
        680, 320, 684, 335, 683, 345,   // brow ridge
        700, 375, 735, 400, 752, 420,   // nose bridge to tip
        760, 430, 752, 442, 738, 444,   // tip to columella
        728, 446, 722, 450, 724, 458,   // philtrum
        738, 466, 744, 476, 738, 484,   // upper lip
        726, 490, 712, 492, 702, 494,   // into the open mouth
        712, 502, 726, 510, 728, 522,   // lower lip, front
        729, 534, 720, 542, 708, 545,   // lower lip, bottom
        702, 552, 704, 562, 712, 575,   // labiomental fold
        722, 595, 718, 615, 700, 628,   // chin
        670, 645, 630, 648, 600, 650,   // under the chin
        612, 700, 622, 760, 630, 830,   // throat
        700, 860, 820, 880, 950, 940 }; // shoulder
    g.frontContour = pathFrom (front, false);

    g.silhouette = g.frontContour;
    g.silhouette.lineTo (u (950, 2000));
    g.silhouette.lineTo (u (100, 2000));
    g.silhouette.lineTo (u (100, 960));
    g.silhouette.cubicTo (u (220, 900), u (360, 880), u (420, 820));   // back shoulder
    g.silhouette.cubicTo (u (432, 760), u (420, 680), u (380, 600));   // nape
    g.silhouette.cubicTo (u (300, 520), u (300, 300), u (400, 200));   // back of the head
    g.silhouette.cubicTo (u (450, 160), u (510, 160), u (560, 170));   // crown
    g.silhouette.closeSubPath();

    g.upperLip = pathFrom ({ 724, 458, 738, 466, 744, 476, 738, 484, 726, 490, 712, 492, 702, 494, 700, 482, 708, 468, 724, 458 }, true);
    g.lowerLip = pathFrom ({ 702, 494, 712, 502, 726, 510, 728, 522, 729, 534, 720, 542, 708, 545, 700, 530, 698, 510, 702, 494 }, true);
    g.mouth = pathFrom ({ 738, 484, 726, 490, 712, 492, 702, 494, 712, 502, 726, 510, 728, 522, 736, 512, 742, 496, 738, 484 }, true);
    g.teeth = pathFrom ({ 736, 486, 728, 490, 716, 492, 706, 494, 709, 496, 712, 498, 714, 498, 724, 496, 732, 492, 736, 486 }, true);
    g.brow = pathFrom ({ 622, 333, 633, 323, 660, 310, 692, 322, 668, 318, 642, 322, 622, 333 }, true);

    // eye: closed lid with lashes, faint crease, nostril
    {
        Path e;
        e.startNewSubPath (u (640, 368));
        e.quadraticTo (u (662, 382), u (688, 366));
        for (int i = 0; i < 5; ++i)
        {
            const float x = 646.0f + 9.0f * (float) i, y = 372.0f + (i == 0 || i == 4 ? -2.0f : 2.0f);
            e.startNewSubPath (u (x, y));
            e.quadraticTo (u (x - 1.0f, y + 9.0f), u (x - 9.0f, y + 15.0f));
        }
        e.startNewSubPath (u (646, 356));
        e.quadraticTo (u (666, 349), u (686, 357));
        e.startNewSubPath (u (722, 432));
        e.quadraticTo (u (731, 440), u (741, 437));
        g.eyeLines = e;
    }
    g.jawLine = pathFrom ({ 698, 628, 640, 600, 580, 580, 540, 520 }, false);

    // Hair: wavy strands between an inner guide (along the face) and an outer guide (volume behind the head)
    {
        const Point<float> inner[7] = { { 565, 168 }, { 520, 200 }, { 500, 350 }, { 500, 500 }, { 495, 650 }, { 440, 800 }, { 470, 980 } };
        const Point<float> outer[7] = { { 560, 165 }, { 400, 60 }, { 200, 160 }, { 200, 450 }, { 195, 700 }, { 140, 850 }, { 120, 1000 } };
        auto bez = [] (Point<float> a, Point<float> b, Point<float> c, Point<float> d, float t) {
            const float m = 1.0f - t;
            return a * (m * m * m) + b * (3.0f * t * m * m) + c * (3.0f * t * t * m) + d * (t * t * t);
        };
        auto strandPoints = [&] (float t, float phase, float endExtra) {
            Point<float> p[7];
            for (int k = 0; k < 7; ++k)
                p[k] = inner[k] + (outer[k] - inner[k]) * t;
            p[6].y += endExtra;
            std::vector<Point<float>> pts;
            const int m = 40;
            for (int k = 0; k <= m; ++k)
            {
                const float v = (float) k / (float) m;
                auto q = v <= 0.5f ? bez (p[0], p[1], p[2], p[3], v * 2.0f) : bez (p[3], p[4], p[5], p[6], (v - 0.5f) * 2.0f);
                const float fall = jmax (0.0f, (q.y - 200.0f) / 800.0f);
                q.x += 48.0f * fall * std::sin (q.y / 120.0f + phase); // waves grow towards the ends
                pts.push_back (q);
            }
            return pts;
        };
        auto toPath = [this] (const std::vector<Point<float>>& pts) {
            Path p;
            p.startNewSubPath (u (pts[0].x, pts[0].y));
            for (size_t k = 1; k < pts.size(); ++k)
                p.lineTo (u (pts[k].x, pts[k].y));
            return p;
        };

        const int n = 30;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) (n - 1);
            g.strands.push_back (toPath (strandPoints (t, 0.35f * t + 0.5f * (float) ((i * 5) % 3), 14.0f * (float) ((i * 7) % 5))).createPathWithRoundedCorners (4.0f * s));
            g.strandWidth.push_back ((0.7f + 1.8f * (float) ((i * 5) % 4) / 3.0f) * s);
            g.strandAlpha.push_back (0.40f + 0.55f * (float) ((i * 3) % 4) / 3.0f);
        }
        auto outerPts = strandPoints (1.0f, 0.35f, 0.0f), innerPts = strandPoints (0.0f, 0.0f, 0.0f);
        g.hairMass = toPath (outerPts);
        for (auto it = innerPts.rbegin(); it != innerPts.rend(); ++it)
            g.hairMass.lineTo (u (it->x, it->y));
        g.hairMass.closeSubPath();
    }

    g.mouthPoint = u (745, 505);
    g.cheek = u (650, 440);
    g.eyeSocket = u (662, 362);
    g.underJaw = u (640, 660);
    singer = std::move (g);
}

void IdentityPanel::renderBackground()
{
    const float scale = jmax (1.0f, Component::getApproximateScaleFactorForComponent (this));
    const int w = getWidth(), h = getHeight();
    cache = Image (Image::ARGB, jmax (1, roundToInt ((float) w * scale)), jmax (1, roundToInt ((float) h * scale)), true);
    Graphics g (cache);
    g.addTransform (AffineTransform::scale (scale));

    const auto r = getLocalBounds().toFloat();
    const float corner = 12.0f;
    Path shape;
    shape.addRoundedRectangle (r.reduced (0.5f), corner);
    g.saveState();
    g.reduceClipRegion (shape);

    g.setGradientFill (ColourGradient (pal::violetDeep, 0, 0, pal::bg, 0, r.getBottom(), false));
    g.fillRect (r);

    const auto c = ringCentre();
    const float base = jmin (r.getWidth(), r.getHeight()) * 0.46f;
    if (face.isValid())
    {
        g.drawImage (face, r, RectanglePlacement::centred | RectanglePlacement::fillDestination);
    }
    else
    {
        const auto& sg = singer;
        const float un = sg.unit;
        auto radial = [&] (Point<float> p, float radius, Colour col) {
            g.setGradientFill (ColourGradient (col, p.x, p.y, col.withAlpha (0.0f), p.x + radius, p.y, true));
            g.fillRect (r);
        };

        // coloured side lighting (violet from the left, magenta from the right)
        g.setGradientFill (ColourGradient (pal::violet.withAlpha (0.30f), r.getX(), r.getHeight() * 0.25f, pal::violet.withAlpha (0.0f), r.getX() + r.getWidth() * 0.6f, r.getHeight() * 0.25f, true));
        g.fillRect (r);
        g.setGradientFill (ColourGradient (pal::rose.withAlpha (0.22f), r.getRight(), r.getHeight() * 0.5f, pal::rose.withAlpha (0.0f), r.getRight() - r.getWidth() * 0.7f, r.getHeight() * 0.5f, true));
        g.fillRect (r);
        // fine scan lines for a holographic texture
        g.setColour (Colours::white.withAlpha (0.022f));
        for (float yy = r.getY() + 2.0f; yy < r.getBottom(); yy += 4.0f)
            g.fillRect (r.getX(), yy, r.getWidth(), 1.0f);
        // static halo rings behind the head
        for (int k = 0; k < 5; ++k)
        {
            const float rr = base * (0.45f + 0.16f * (float) k);
            g.setColour ((k % 2 == 0 ? pal::rose : pal::violet).withAlpha (0.11f - 0.015f * (float) k));
            g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 1.0f);
        }

        // 1) skin: translucent plane, dark at the back, magenta where the light hits the profile
        const auto back = u (350, 400), frontPt = u (780, 430);
        g.setGradientFill (ColourGradient (Colour (0xf2241238), back.x, back.y, Colour (0xd9b8337a), frontPt.x, frontPt.y, false));
        g.fillPath (sg.silhouette);

        // 2) soft modelling inside the face: cheek light, eye socket and under-jaw shadows
        g.saveState();
        g.reduceClipRegion (sg.silhouette);
        radial (sg.cheek, 110.0f * un, pal::roseSoft.withAlpha (0.32f));
        radial (sg.eyeSocket, 48.0f * un, pal::bg.withAlpha (0.42f));
        radial (sg.underJaw, 95.0f * un, pal::bg.withAlpha (0.55f));
        radial (u (700, 520), 60.0f * un, pal::rose.withAlpha (0.22f)); // warm glow around the mouth
        {
            const auto f0 = u (0, 820), f1 = u (0, 1000);
            g.setGradientFill (ColourGradient (pal::bg.withAlpha (0.0f), 0, f0.y, pal::bg.withAlpha (0.85f), 0, f1.y, false));
            g.fillRect (r);
        }
        g.restoreState();

        // 3) open mouth: dark interior with a hint of teeth
        g.setColour (Colour (0xff1a0820));
        g.fillPath (sg.mouth);
        g.setColour (Colour (0x99ffe6f2));
        g.fillPath (sg.teeth);

        // 4) neon rim light on the profile: layered soft glow, then a bright violet->rose line
        for (auto [wd, al] : { std::pair<float, float> { 16.0f, 0.05f }, { 10.0f, 0.09f }, { 6.0f, 0.16f } })
        {
            g.setColour (pal::rose.withAlpha (al));
            g.strokePath (sg.frontContour, PathStrokeType (wd * un * 1.6f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        const auto top = u (600, 180), chin = u (700, 640);
        g.setGradientFill (ColourGradient (pal::violet, top.x, top.y, pal::rose, chin.x, chin.y, false));
        g.strokePath (sg.frontContour, PathStrokeType (jmax (1.2f, 3.2f * un), PathStrokeType::curved, PathStrokeType::rounded));

        // 5) lips with gloss, brow, eye and nose details
        const auto lipA = u (700, 460), lipB = u (735, 540);
        g.setGradientFill (ColourGradient (pal::rose, lipA.x, lipA.y, pal::roseSoft, lipB.x, lipB.y, false));
        g.fillPath (sg.upperLip);
        g.fillPath (sg.lowerLip);
        {
            Path gloss;
            gloss.startNewSubPath (u (716, 512));
            gloss.quadraticTo (u (722, 520), u (720, 530));
            g.setColour (Colours::white.withAlpha (0.6f));
            g.strokePath (gloss, PathStrokeType (jmax (1.0f, 2.0f * un), PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (pal::roseSoft.withAlpha (0.85f));
        g.fillPath (sg.brow);
        g.setColour (Colour (0xffffd6ea));
        g.strokePath (sg.eyeLines, PathStrokeType (jmax (1.0f, 2.4f * un), PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (pal::violet.withAlpha (0.35f));
        g.strokePath (sg.jawLine, PathStrokeType (jmax (1.0f, 2.0f * un), PathStrokeType::curved, PathStrokeType::rounded));

        // 6) hair: translucent volume, then luminous flowing strands (rose -> violet)
        const auto hTop = u (600, 120), hBottom = u (180, 1000);
        g.setGradientFill (ColourGradient (Colour (0xf25a2a7a), hTop.x, hTop.y, Colour (0xe61a0e2a), hBottom.x, hBottom.y, false));
        g.fillPath (sg.hairMass);
        ColourGradient strandGrad (pal::roseSoft, hTop.x, hTop.y, pal::violet, hBottom.x, hBottom.y, false);
        strandGrad.addColour (0.45, pal::rose);
        for (size_t i = 0; i < sg.strands.size(); i += 4)
        {
            auto gr = strandGrad;
            gr.multiplyOpacity (0.12f);
            g.setGradientFill (gr);
            g.strokePath (sg.strands[i], PathStrokeType (jmax (3.0f, sg.strandWidth[i] * 5.0f), PathStrokeType::curved, PathStrokeType::rounded));
        }
        for (size_t i = 0; i < sg.strands.size(); ++i)
        {
            auto gr = strandGrad;
            gr.multiplyOpacity (sg.strandAlpha[i]);
            g.setGradientFill (gr);
            g.strokePath (sg.strands[i], PathStrokeType (jmax (0.6f, sg.strandWidth[i]), PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    // bottom fade so the route selector reads clearly, and a top sheen
    g.setGradientFill (ColourGradient (pal::bg.withAlpha (0.0f), 0, r.getHeight() * 0.62f, pal::bg.withAlpha (0.92f), 0, r.getBottom(), false));
    g.fillRect (r);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), 0, 0, Colours::white.withAlpha (0.0f), 0, 40.0f, false));
    g.fillRect (r.withHeight (40.0f));
    g.restoreState();

    g.setColour (pal::panelEdge.brighter (0.1f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

void IdentityPanel::paint (Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;
    if (! cache.isValid())
        renderBackground();
    g.drawImage (cache, getLocalBounds().toFloat());

    Path clip;
    clip.addRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 12.0f);
    g.reduceClipRegion (clip);

    // animated halo: rings open and brighten with the real output level
    const auto c = ringCentre();
    const float base = jmin ((float) getWidth(), (float) getHeight()) * 0.46f;
    for (int k = 0; k < 4; ++k)
    {
        const float rr = base * (0.45f + 0.17f * (float) k) + level * base * 0.05f * (float) (k + 1);
        const float a = (0.10f + 0.45f * level) * (1.0f - 0.22f * (float) k);
        const auto col = k % 2 == 0 ? pal::rose : pal::violet;
        g.setColour (col.withAlpha (a * 0.3f));
        g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 4.0f);
        g.setColour (col.withAlpha (a));
        g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 1.2f);
    }
    if (face.isValid() || level < 0.002f)
        return;

    // the face lights up with the voice: rim light, lips and sound waves leaving the mouth (no lip sync)
    const auto& sg = singer;
    const float un = sg.unit;
    g.setColour (pal::rose.withAlpha (0.25f * level));
    g.strokePath (sg.frontContour, PathStrokeType ((6.0f + 10.0f * level) * un, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (pal::roseSoft.withAlpha (0.35f * level));
    g.fillPath (sg.upperLip);
    g.fillPath (sg.lowerLip);
    for (int k = 0; k < 3; ++k)
    {
        const float rr = (28.0f + 22.0f * (float) k + 10.0f * level) * un;
        Path arc;
        arc.addCentredArc (sg.mouthPoint.x, sg.mouthPoint.y, rr, rr, 0.0f, degreesToRadians (50.0f), degreesToRadians (130.0f), true);
        g.setColour (pal::rose.withAlpha (jlimit (0.0f, 1.0f, level * (0.85f - 0.22f * (float) k))));
        g.strokePath (arc, PathStrokeType (jmax (1.2f, 2.5f * un), PathStrokeType::curved, PathStrokeType::rounded));
    }
}

// ---------------------------------------------------------------------------
VocalShiftEditor::VocalShiftEditor (VocalShiftProcessor& p)
    : AudioProcessorEditor (p), proc (p), state (p.getState())
{
    setLookAndFeel (&look);

    title.setText ("VOCALSHIFT", dontSendNotification);
    title.setFont (uiFont (23.0f, true, 0.12f));
    subtitle.setText (u8s ("AI STUDIO  ·  v" VS_VERSION_STRING "  ·  DSP, sin IA neuronal"), dontSendNotification);
    subtitle.setColour (Label::textColourId, pal::rose);
    subtitle.setFont (uiFont (10.5f, true, 0.14f));
    for (auto* l : { &title, &subtitle })
    {
        l->setBorderSize ({});
        l->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (l);
    }

    easyTab.setButtonText (u8s ("FÁCIL"));
    proTab.setButtonText ("PRO");
    addAndMakeVisible (easyTab);
    addAndMakeVisible (proTab);
    easyTab.onClick = [this] { setUiMode (false); };
    proTab.onClick = [this] { setUiMode (true); };

    bypassButton.setButtonText ("Bypass");
    bypassAttachment = std::make_unique<ButtonAttachment> (state, params::bypass, bypassButton);
    addAndMakeVisible (bypassButton);

    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.setJustificationType (Justification::centred);
    presetBox.onChange = [this] { onPresetChosen(); };
    addAndMakeVisible (presetBox);
    saveButton.setButtonText ("Guardar");
    loadButton.setButtonText ("Cargar");
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

    identityButton.setButtonText ("Identidad");
    radicalButton.setButtonText ("Radical");
    identityButton.setTooltip ("Conservar identidad: los formantes se mantienen cerca de los originales.");
    radicalButton.setTooltip (u8s ("Transformación radical: los formantes siguen más al tono."));
    identityButton.onClick = [this] { setVoiceMode (VoiceMode::keepIdentity); };
    radicalButton.onClick = [this] { setVoiceMode (VoiceMode::radical); };
    transformButton.setButtonText ("Transformar");
    secondVoiceButton.setButtonText ("Segunda voz");
    transformButton.onClick = [this] { setRoute (VoiceRoute::transform); };
    secondVoiceButton.onClick = [this] { setRoute (VoiceRoute::secondVoice); };
    pitchDownButton.setButtonText (u8s ("−12"));
    pitchUpButton.setButtonText ("+12");
    pitchDownButton.onClick = [this] { nudgePitch (-12.0f); };
    pitchUpButton.onClick = [this] { nudgePitch (12.0f); };
    addChildComponent (identity);
    for (auto* b : { &identityButton, &radicalButton, &transformButton, &secondVoiceButton })
        addAndMakeVisible (b);
    addChildComponent (pitchDownButton);
    addChildComponent (pitchUpButton);
    modeDescription.setColour (Label::textColourId, pal::dim);
    modeDescription.setFont (uiFont (12.0f));
    modeDescription.setJustificationType (Justification::topLeft);
    addAndMakeVisible (modeDescription);
    status.setColour (Label::textColourId, pal::dim);
    status.setFont (uiFont (11.5f));
    status.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (status);

    // The three macros
    setupKnob (pitchKnob, "PITCH", u8s ("Altura de la voz procesada (semitonos)."), params::semitones, true, pal::rose);
    setupKnob (characterKnob, u8s ("CARÁCTER"), u8s ("Cuerpo, presencia, color y textura."), params::character, true, pal::violet);
    setupKnob (magicKnob, "MAGIA", u8s ("Espacio y acabado: anchura, reverb, eco."), params::magic, true, pal::rose);

    // Fader
    fader.setSliderStyle (Slider::LinearVertical);
    fader.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 22);
    fader.setColour (Slider::rotarySliderFillColourId, pal::rose);
    fader.setColour (Slider::textBoxTextColourId, pal::text);
    fader.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    fader.setColour (Slider::textBoxBackgroundColourId, Colours::black.withAlpha (0.3f));
    faderAttachment = std::make_unique<SliderAttachment> (state, params::secondGain, fader);
    fader.setDoubleClickReturnValue (true, -12.0);
    fader.onDoubleClickReset = [this] { return restoreFaderToPreset(); };
    faderLabel.setText (u8s ("2.ª VOZ"), dontSendNotification);
    faderLabel.setJustificationType (Justification::centred);
    faderLabel.setFont (uiFont (11.5f, true, 0.1f));
    faderLabel.setColour (Label::textColourId, pal::dim.brighter (0.3f));
    addChildComponent (fader);
    addChildComponent (faderLabel);
    addChildComponent (secondMeter);

    // Pro
    setupKnob (proFine, "Ajuste fino", {}, params::fineCents, false, pal::rose);
    setupKnob (proFormant, "Formantes", {}, params::formantShift, false, pal::violet);
    setupKnob (proBrightness, "Brillo", {}, params::brightness, false, pal::violet);
    setupKnob (proWidth, "Anchura", {}, params::spaceWidth, false, pal::rose);
    setupKnob (proReverb, "Reverb", {}, params::spaceReverb, false, pal::rose);
    setupKnob (proDelay, "Eco corto", {}, params::spaceDelay, false, pal::rose);
    setupKnob (proMix, "Mezcla", {}, params::mix, false, pal::violet);
    setupKnob (proPan, u8s ("Pan 2.ª voz"), {}, params::pan, false, pal::violet);
    setupKnob (proOffset, u8s ("Desfase"), {}, params::offsetMs, false, pal::violet);
    setupKnob (proGain, "Ganancia salida", {}, params::outputGain, false, pal::violet);
    setupToggle (autoLevelButton, autoLevelAttachment, u8s ("Nivel automático"), params::autoLevel);
    setupToggle (safetyButton, safetyAttachment, u8s ("Protección de picos"), params::safetyClip);
    setupToggle (spatialButton, spatialAttachment, u8s ("Espacio (2.ª voz)"), params::spatial);
    setupToggle (improvedButton, improvedAttachment, u8s ("DSP mejorado (apagado = versiones anteriores)"), params::improved);
    setupToggle (engineButton, engineAttachment, u8s ("Motor experimental PSOLA (sin validar al oído)"), params::engine);
    engineInfo.setColour (Label::textColourId, pal::dim);
    engineInfo.setFont (uiFont (12.0f));
    engineInfo.setJustificationType (Justification::topLeft);
    addChildComponent (engineInfo);

    roadmap.setText (u8s ("Pendiente (sin controles): IA neuronal · Armonías · Edición por frases · Modo Live de baja latencia"), dontSendNotification);
    roadmap.setColour (Label::textColourId, pal::dim.withAlpha (0.6f));
    roadmap.setFont (uiFont (10.5f));
    addAndMakeVisible (roadmap);

    for (auto* m : { &inMeter, &outMeter })
    {
        m->onClipClicked = [this] { proc.clearClipIndicators(); };
        addAndMakeVisible (m);
    }

    rebuildPresetMenu();
    proMode = state.state.getProperty (params::propUiMode, "easy").toString() == "pro";
    shownVoiceMode = voiceMode();
    shownRoute = route();
    shownImproved = state.getRawParameterValue (params::improved)->load() >= 0.5f;
    refreshModeButtons();
    setUiMode (proMode);

    const int w = state.state.getProperty (params::propUiWidth, 940);
    const int h = state.state.getProperty (params::propUiHeight, 700);
    setResizable (true, true);
    setResizeLimits (800, 640, 1700, 1100);
    setSize (jlimit (800, 1700, w), jlimit (640, 1100, h));

    timerCallback();
    startTimerHz (30);
}

VocalShiftEditor::~VocalShiftEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VocalShiftEditor::setupKnob (Knob& k, const String& text, const String& hint, const char* paramId, bool big, Colour colour)
{
    k.slider.getProperties().set ("vsBig", big);
    if (big)
        k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0); // the value is drawn inside the knob
    else
        k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (Slider::rotarySliderFillColourId, colour);
    k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k.slider.setColour (Slider::textBoxBackgroundColourId, Colours::black.withAlpha (0.28f));
    k.slider.setColour (Slider::textBoxTextColourId, pal::text);
    k.slider.setRotaryParameters (degreesToRadians (225.0f), degreesToRadians (495.0f), true);
    k.slider.setVelocityBasedMode (false);
    k.slider.setScrollWheelEnabled (true);
    k.slider.setPopupDisplayEnabled (big, false, this);
    k.label.setText (text, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (big ? uiFont (13.5f, true, 0.12f) : uiFont (11.5f, false, 0.03f));
    k.label.setColour (Label::textColourId, big ? pal::text : pal::dim.brighter (0.35f));
    k.label.setInterceptsMouseClicks (false, false);
    k.hint.setText (hint, dontSendNotification);
    k.hint.setJustificationType (Justification::centredTop);
    k.hint.setColour (Label::textColourId, pal::dim);
    k.hint.setFont (uiFont (11.5f));
    k.hint.setInterceptsMouseClicks (false, false);
    addChildComponent (k.slider);
    addChildComponent (k.label);
    addChildComponent (k.hint);
    k.attachment = std::make_unique<SliderAttachment> (state, paramId, k.slider); // also sets double click = parameter default
}

void VocalShiftEditor::setupToggle (ToggleButton& b, std::unique_ptr<ButtonAttachment>& att, const String& text, const char* paramId)
{
    b.setButtonText (text);
    att = std::make_unique<ButtonAttachment> (state, paramId, b);
    addChildComponent (b);
}

void VocalShiftEditor::nudgePitch (float semis)
{
    if (auto* param = state.getParameter (params::semitones))
    {
        const float now = state.getRawParameterValue (params::semitones)->load();
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (jlimit (-24.0f, 24.0f, now + semis)));
        param->endChangeGesture();
    }
}

VoiceMode VocalShiftEditor::voiceMode() const
{
    return state.getRawParameterValue (params::mode)->load() >= 0.5f ? VoiceMode::radical : VoiceMode::keepIdentity;
}

VoiceRoute VocalShiftEditor::route() const
{
    return state.getRawParameterValue (params::route)->load() >= 0.5f ? VoiceRoute::secondVoice : VoiceRoute::transform;
}

void VocalShiftEditor::setVoiceMode (VoiceMode m)
{
    if (auto* param = state.getParameter (params::mode))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) (int) m));
        param->endChangeGesture();
    }
    shownVoiceMode = m;
    refreshModeButtons();
}

void VocalShiftEditor::setRoute (VoiceRoute r)
{
    if (auto* param = state.getParameter (params::route))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) (int) r));
        param->endChangeGesture();
    }
    shownRoute = r;
    refreshModeButtons();
}

bool VocalShiftEditor::restoreFaderToPreset()
{
    // Double click: the value of the current factory preset, or the parameter default for user presets.
    float target = -12.0f;
    if (const auto* fp = findFactoryPreset (proc.getPresetManager().currentPresetId().toStdString()))
        target = fp->params.secondGainDb;
    if (auto* param = state.getParameter (params::secondGain))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (target));
        param->endChangeGesture();
    }
    return true;
}

void VocalShiftEditor::refreshModeButtons()
{
    const bool radical = shownVoiceMode == VoiceMode::radical;
    const bool second = shownRoute == VoiceRoute::secondVoice;
    identityButton.setToggleState (! radical, dontSendNotification);
    radicalButton.setToggleState (radical, dontSendNotification);
    transformButton.setToggleState (! second, dontSendNotification);
    secondVoiceButton.setToggleState (second, dontSendNotification);
    characterKnob.slider.setColour (Slider::rotarySliderFillColourId, radical ? pal::rose : pal::violet);
    for (auto* k : { &proMix })
        k->slider.setEnabled (! second);
    for (auto* k : { &proPan, &proOffset, &proWidth, &proReverb, &proDelay })
        k->slider.setEnabled (second);
    spatialButton.setEnabled (second);

    String d;
    if (second)
        d << (state.getRawParameterValue (params::spatial)->load() >= 0.5f && shownImproved
                  ? u8s ("Segunda voz: tu voz queda al frente sin cambios; detrás, una voz procesada ancha y con espacio (Magia). ")
                  : u8s ("Segunda voz: tu voz queda al frente sin cambios; detrás, la voz procesada. "));
    else
        d << u8s ("Transformar: la voz procesada reemplaza a la original. ");
    d << (radical ? u8s ("Radical: los formantes siguen más al tono. Sigue siendo DSP, no conversión por IA.")
                  : u8s ("Identidad: los formantes se mantienen cerca de los originales."));
    modeDescription.setText (d, dontSendNotification);
    layoutVisibility();
    resized();
    repaint();
}

void VocalShiftEditor::layoutVisibility()
{
    const bool second = shownRoute == VoiceRoute::secondVoice;
    for (auto* c : { (Component*) &fader, (Component*) &faderLabel, (Component*) &secondMeter })
        c->setVisible (second);
}

void VocalShiftEditor::setUiMode (bool pro)
{
    proMode = pro;
    state.state.setProperty (params::propUiMode, pro ? "pro" : "easy", nullptr);
    easyTab.setToggleState (! pro, dontSendNotification);
    proTab.setToggleState (pro, dontSendNotification);

    for (auto* k : { &pitchKnob, &characterKnob, &magicKnob })
    {
        k->slider.setVisible (true);
        k->label.setVisible (true);
        k->hint.setVisible (! pro);
    }
    pitchKnob.label.setVisible (pro); // in Easy the PITCH panel title names it
    for (auto* k : { &proFine, &proFormant, &proBrightness, &proMix, &proGain, &proPan, &proOffset, &proWidth, &proReverb, &proDelay })
    {
        k->slider.setVisible (pro);
        k->label.setVisible (pro);
    }
    modeDescription.setVisible (! pro);
    identity.setVisible (! pro);
    pitchDownButton.setVisible (! pro);
    pitchUpButton.setVisible (! pro);
    for (auto* b : { &safetyButton, &autoLevelButton, &spatialButton })
        b->setVisible (true);
    for (auto* b : { &improvedButton, &engineButton })
        b->setVisible (pro);
    engineInfo.setVisible (pro);
    layoutVisibility();
    resized();
    repaint();
}

void VocalShiftEditor::rebuildPresetMenu()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading (u8s ("Presets de fábrica (DSP)"));
    const auto& fp = factoryPresets();
    for (size_t i = 0; i < fp.size(); ++i)
        presetBox.addItem (u8s (fp[i].name.c_str()), (int) i + factoryIdOffset);

    userPresetFiles = proc.getPresetManager().listUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("Mis presets");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), i + userIdOffset);
    }
    shownPresetId = {};
}

void VocalShiftEditor::syncPresetBox()
{
    const auto presetId = proc.getPresetManager().currentPresetId();
    if (presetId == shownPresetId)
        return;
    shownPresetId = presetId;
    const auto& fp = factoryPresets();
    int selected = 0;
    for (size_t i = 0; i < fp.size(); ++i)
        if (presetId == String (fp[i].id))
            selected = (int) i + factoryIdOffset;
    if (presetId.startsWith ("user:"))
        for (int i = 0; i < userPresetFiles.size(); ++i)
            if (userPresetFiles[i].getFileNameWithoutExtension() == presetId.fromFirstOccurrenceOf ("user:", false, false))
                selected = i + userIdOffset;
    presetBox.setSelectedId (selected, dontSendNotification);
}

void VocalShiftEditor::onPresetChosen()
{
    const int id = presetBox.getSelectedId();
    if (id >= userIdOffset)
    {
        const int idx = id - userIdOffset;
        if (isPositiveAndBelow (idx, userPresetFiles.size()))
        {
            auto r = proc.getPresetManager().loadUserPreset (userPresetFiles[idx]);
            if (r.failed())
                status.setText (r.getErrorMessage(), dontSendNotification);
        }
    }
    else if (id >= factoryIdOffset)
    {
        const auto& fp = factoryPresets();
        const size_t idx = (size_t) (id - factoryIdOffset);
        if (idx < fp.size())
            proc.getPresetManager().applyFactoryPreset (fp[idx].id);
    }
}

void VocalShiftEditor::savePreset()
{
    auto dir = PresetManager::userPresetDirectory();
    dir.createDirectory();
    chooser = std::make_unique<FileChooser> ("Guardar preset", dir, String ("*") + PresetManager::fileExtension);
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
        [this] (const FileChooser& fc) {
            auto file = fc.getResult();
            if (file == File())
                return;
            file = file.withFileExtension (PresetManager::fileExtension);
            auto r = proc.getPresetManager().saveUserPreset (file, file.getFileNameWithoutExtension());
            status.setText (r.wasOk() ? "Preset guardado: " + file.getFileName() : r.getErrorMessage(), dontSendNotification);
            rebuildPresetMenu();
        });
}

void VocalShiftEditor::loadPreset()
{
    chooser = std::make_unique<FileChooser> ("Cargar preset", PresetManager::userPresetDirectory(),
                                             String ("*") + PresetManager::fileExtension);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
        [this] (const FileChooser& fc) {
            auto file = fc.getResult();
            if (! file.existsAsFile())
                return;
            auto r = proc.getPresetManager().loadUserPreset (file);
            status.setText (r.wasOk() ? "Preset cargado: " + file.getFileName() : r.getErrorMessage(), dontSendNotification);
        });
}

String VocalShiftEditor::effectiveSummary() const
{
    const auto e = mapMacros (proc.getCurrentVoiceParams());
    if (state.getRawParameterValue (params::bypass)->load() >= 0.5f)
        return "BYPASS activo (salida = entrada, alineada en latencia)";
    String s;
    s << "Efectivo: tono " << (e.semitones >= 0 ? "+" : "") << String (e.semitones, 2) << " st"
      << u8s (" · formantes ") << String (e.formantShiftSemitones, 1) << " st"
      << u8s (" · brillo ") << String (e.brightnessDb, 1) << " dB";
    if (e.spatialAmount > 0.0f)
        s << u8s (" · anchura ") << String (roundToInt (e.spaceWidth * 100.0f)) << u8s (" % · reverb ") << String (roundToInt (e.spaceReverb * 100.0f))
          << u8s (" % (RT60 ") << String (e.spaceRt60, 1) << u8s (" s) · eco ") << String (roundToInt (e.spaceSlap * 100.0f)) << u8s (" % · cuerpo/presencia ") << String (roundToInt (e.textureAmount * 100.0f)) << " %";
    else
        s << u8s (" · compresión ") << String (1.0f + 3.0f * e.magic01, 1) << ":1, aire +" << String (5.0f * e.magic01, 1) << " dB";
    return s;
}

void VocalShiftEditor::timerCallback()
{
    inMeter.setLevel (proc.consumeInputPeak());
    const float outPeak = proc.consumeOutputPeak();
    outMeter.setLevel (outPeak);
    if (identity.isVisible())
        identity.setLevel (outPeak);
    secondMeter.setLevel (proc.consumeSecondVoicePeak());
    inMeter.setClipped (proc.inputClipped());
    outMeter.setClipped (proc.outputClipped());

    const auto mode = voiceMode();
    const auto rt = route();
    const bool imp = state.getRawParameterValue (params::improved)->load() >= 0.5f;
    if (mode != shownVoiceMode || rt != shownRoute || imp != shownImproved)
    {
        shownVoiceMode = mode;
        shownRoute = rt;
        shownImproved = imp;
        refreshModeButtons();
    }
    syncPresetBox();

    if (! status.getText().startsWith ("Preset"))
        status.setText (effectiveSummary(), dontSendNotification);

    if (proMode)
    {
        const double sr = proc.getPreparedSampleRate();
        const int lat = proc.getEngineLatencySamples();
        String info;
        info << "Motor: " << proc.getEngineName() << u8s (proc.getEngineName()[0] == 'P' ? " (EXPERIMENTAL, sin validar al oído)\n" : " (calidad, bloque 120 ms)\n")
             << "Latencia reportada al host: " << lat << " muestras";
        if (sr > 0)
            info << " (" << String (1000.0 * lat / sr, 1) << " ms a " << String (sr / 1000.0, 1) << " kHz)";
        info << u8s ("\nNivel automático: ") << String (proc.getAutoLevelGainDb(), 1) << u8s (" dB · espacio: ") << (proc.getSpatialActive() ? "activo" : "inactivo");
        info << u8s ("\nNo apto para monitoreo en vivo: no existe un modo de baja latencia.");
        if (proc.getNonFiniteEvents() > 0)
            info << u8s ("\nAviso: se silenciaron ") << proc.getNonFiniteEvents() << u8s (" bloques con valores no finitos.");
        engineInfo.setText (info, dontSendNotification);
    }
}

void VocalShiftEditor::paintPanel (Graphics& g, Rectangle<int> area, const String& text, Colour accent) const
{
    if (area.isEmpty())
        return;
    auto r = area.toFloat();
    const float corner = 12.0f;
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), corner);
    g.setGradientFill (ColourGradient (Colour (0xff1c1729), 0, r.getY(), Colour (0xff110e19), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (pal::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
    if (text.isNotEmpty())
    {
        auto t = r.reduced (12.0f, 0.0f).withHeight (30.0f).translated (0.0f, 4.0f);
        g.setColour (pal::text.withAlpha (0.82f));
        g.setFont (uiFont (12.5f, true, 0.18f));
        g.drawText (text, t, Justification::centred);
        const float tw = 18.0f;
        g.setGradientFill (ColourGradient (pal::violet, t.getCentreX() - tw, 0, accent, t.getCentreX() + tw, 0, false));
        g.fillRoundedRectangle (Rectangle<float> (tw * 2.0f, 2.0f).withCentre ({ t.getCentreX(), t.getBottom() - 2.0f }), 1.0f);
    }
}

void VocalShiftEditor::paint (Graphics& g)
{
    g.fillAll (pal::bg);
    const auto b = getLocalBounds().toFloat();
    // deep violet glow behind everything, strongest at the top centre
    g.setGradientFill (ColourGradient (pal::violetDeep.withAlpha (0.85f), b.getCentreX(), 40.0f, pal::bg.withAlpha (0.0f), b.getCentreX(), b.getHeight() * 0.8f, true));
    g.fillRect (b);

    // header bar with a neon base line
    auto top = b.withHeight (64.0f);
    g.setGradientFill (ColourGradient (Colour (0xff15111f), 0, 0, Colour (0xff0c0a13), 0, top.getBottom(), false));
    g.fillRect (top);
    g.setGradientFill (ColourGradient (pal::violet.withAlpha (0.0f), 0, 0, pal::rose.withAlpha (0.9f), b.getCentreX(), 0, false));
    g.fillRect (top.withY (top.getBottom() - 1.5f).withHeight (1.5f).withWidth (b.getCentreX()));
    g.setGradientFill (ColourGradient (pal::rose.withAlpha (0.9f), b.getCentreX(), 0, pal::violet.withAlpha (0.0f), b.getRight(), 0, false));
    g.fillRect (Rectangle<float> (b.getCentreX(), top.getBottom() - 1.5f, b.getCentreX(), 1.5f));

    if (proMode)
    {
        for (int i = 0; i < 6; ++i)
            paintPanel (g, proPanels[i], u8s (proPanelTitles[i]), i % 2 == 0 ? pal::rose : pal::violet);
    }
    else
    {
        paintPanel (g, easyVoicePanel, "VOZ", pal::violet);
        paintPanel (g, easyPitchPanel, "PITCH", pal::rose);
        // pitch scale around the big knob (same geometry as the LookAndFeel)
        if (! pitchKnobArea.isEmpty())
        {
            const auto kb = pitchKnobArea.toFloat();
            const float d = jmin (kb.getWidth(), kb.getHeight());
            const auto c = kb.getCentre();
            const float rLabel = d * 0.5f * 0.89f + d * 0.5f * 0.89f * 0.075f * 1.7f + 12.0f;
            g.setFont (uiFont (10.5f, true));
            const float start = degreesToRadians (225.0f), end = degreesToRadians (495.0f);
            int i = 0;
            for (auto* lab : { "-24", "-12", "0", "+12", "+24" })
            {
                const float a = start + (float) i++ / 4.0f * (end - start);
                const auto pnt = c.getPointOnCircumference (rLabel, a);
                g.setColour (pal::dim);
                g.drawText (u8s (lab), Rectangle<float> (34.0f, 14.0f).withCentre (pnt), Justification::centred);
            }
        }
    }
    if (shownRoute == VoiceRoute::secondVoice)
        paintPanel (g, faderStrip, {}, pal::rose);

    // footer separator
    g.setColour (pal::panelEdge);
    g.drawHorizontalLine (getHeight() - 46, 12.0f, b.getRight() - 12.0f);
}

void VocalShiftEditor::resized()
{
    state.state.setProperty (params::propUiWidth, getWidth(), nullptr);
    state.state.setProperty (params::propUiHeight, getHeight(), nullptr);

    auto r = getLocalBounds();
    // header: brand | preset (centre) | save/load | Easy/Pro | bypass
    auto header = r.removeFromTop (64).reduced (16, 12);
    auto titleArea = header.removeFromLeft (230);
    title.setBounds (titleArea.removeFromTop (26));
    subtitle.setBounds (titleArea);
    bypassButton.setBounds (header.removeFromRight (84));
    header.removeFromRight (8);
    auto tabs = header.removeFromRight (132).reduced (0, 3);
    proTab.setBounds (tabs.removeFromRight (66).reduced (1, 0));
    easyTab.setBounds (tabs.reduced (1, 0));
    header.removeFromRight (14);
    loadButton.setBounds (header.removeFromRight (66).reduced (0, 4));
    header.removeFromRight (4);
    saveButton.setBounds (header.removeFromRight (66).reduced (0, 4));
    header.removeFromRight (10);
    presetBox.setBounds (header.withSizeKeepingCentre (jmin (300, header.getWidth()), 32));

    r.reduce (12, 10);
    // footer: status + roadmap
    auto footer = r.removeFromBottom (34);
    status.setBounds (footer.removeFromTop (18));
    roadmap.setBounds (footer);
    r.removeFromBottom (10);

    // edge meters, like hardware: IN on the left, OUT on the right
    inMeter.setBounds (r.removeFromLeft (26));
    r.removeFromLeft (10);
    outMeter.setBounds (r.removeFromRight (26));
    r.removeFromRight (10);

    const bool second = shownRoute == VoiceRoute::secondVoice;
    Rectangle<int> faderArea;
    if (second)
    {
        faderArea = r.removeFromRight (jlimit (104, 140, r.getWidth() / 7));
        r.removeFromRight (10);
    }

    easyVoicePanel = easyPitchPanel = pitchKnobArea = {};
    for (auto& p : proPanels) p = {};

    if (! proMode)
    {
        const int gap = 10;
        const int total = r.getWidth() - 2 * gap;
        const int wl = total * 31 / 100, wc = total * 37 / 100;
        easyVoicePanel = r.removeFromLeft (wl);
        r.removeFromLeft (gap);
        auto centre = r.removeFromLeft (wc);
        r.removeFromLeft (gap);
        easyPitchPanel = r;

        // centre: identity panel with the route selector floating at its foot
        identity.setBounds (centre);
        {
            auto rb = centre.reduced (14).removeFromBottom (32);
            rb = rb.withSizeKeepingCentre (jmin (rb.getWidth(), 260), rb.getHeight());
            transformButton.setBounds (rb.removeFromLeft (rb.getWidth() / 2).reduced (2, 0));
            secondVoiceButton.setBounds (rb.reduced (2, 0));
        }

        // left: VOZ (mode, description, Caracter and Magia)
        {
            auto a = easyVoicePanel.reduced (14, 10);
            a.removeFromTop (34);
            auto modeRow = a.removeFromTop (30);
            identityButton.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2).reduced (2, 0));
            radicalButton.setBounds (modeRow.reduced (2, 0));
            a.removeFromTop (8);
            modeDescription.setBounds (a.removeFromTop (jlimit (40, 64, a.getHeight() / 5)));
            a.removeFromTop (4);
            // knob + name + hint as one group, centred vertically in what is left
            const int w = a.getWidth() / 2;
            const int side = jmax (60, jmin (w - 4, a.getHeight() - 70, 200));
            auto group = a.withSizeKeepingCentre (a.getWidth(), jmin (a.getHeight(), side + 70));
            for (auto* k : { &characterKnob, &magicKnob })
            {
                auto cell = group.removeFromLeft (w).reduced (2, 0);
                k->slider.setBounds (cell.removeFromTop (side).withSizeKeepingCentre (side, side));
                k->label.setBounds (cell.removeFromTop (22));
                k->hint.setBounds (cell);
            }
        }

        // right: PITCH (big knob with scale, -12/+12, switches)
        {
            auto a = easyPitchPanel.reduced (14, 10);
            a.removeFromTop (34);
            auto row = a.removeFromTop (26);
            row = row.withSizeKeepingCentre (jmin (row.getWidth(), 124), row.getHeight());
            pitchDownButton.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (3, 0));
            pitchUpButton.setBounds (row.reduced (3, 0));
            auto toggles = a.removeFromBottom (84);
            const int th = toggles.getHeight() / 3;
            for (auto* t : { (Component*) &spatialButton, (Component*) &autoLevelButton, (Component*) &safetyButton })
                t->setBounds (toggles.removeFromTop (th));
            a.removeFromBottom (6);
            const int side = jmax (60, jmin (a.getWidth() - 44, a.getHeight() - 76, 300));
            auto group = a.withSizeKeepingCentre (a.getWidth(), jmin (a.getHeight(), side + 66));
            group.removeFromTop (22); // room for the "0" scale label
            pitchKnobArea = group.removeFromTop (side).withSizeKeepingCentre (side, side);
            pitchKnob.slider.setBounds (pitchKnobArea);
            pitchKnob.hint.setBounds (group.withTrimmedTop (16));
        }
    }
    else
    {
        // route and mode selectors on top of the Pro grid
        auto row2 = r.removeFromTop (30);
        const int bw = jmin (150, (row2.getWidth() - 24) / 4);
        transformButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
        secondVoiceButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
        row2.removeFromLeft (22);
        identityButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
        radicalButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
        r.removeFromTop (10);

        auto grid = r;
        auto rowTop = grid.removeFromTop (grid.getHeight() * 45 / 100);
        grid.removeFromTop (8);
        auto rowMid = grid.removeFromTop (grid.getHeight() * 55 / 100);
        grid.removeFromTop (8);
        auto rowBottom = grid;

        auto place = [] (Rectangle<int> area, std::initializer_list<Knob*> ks, int headerH = 32) {
            area.removeFromTop (headerH);
            area.reduce (6, 4);
            const int w = area.getWidth() / (int) ks.size();
            for (auto* k : ks)
            {
                auto cell = area.removeFromLeft (w).reduced (3, 0);
                k->label.setBounds (cell.removeFromTop (16));
                k->slider.setBounds (cell);
            }
        };
        // row 1: Pitch | Timbre | Dinámica
        const int w1 = rowTop.getWidth();
        proPanels[0] = rowTop.removeFromLeft (w1 * 28 / 100);
        rowTop.removeFromLeft (8);
        proPanels[1] = rowTop.removeFromLeft (w1 * 37 / 100);
        rowTop.removeFromLeft (8);
        proPanels[2] = rowTop;
        place (proPanels[0], { &pitchKnob, &proFine });
        place (proPanels[1], { &characterKnob, &proFormant, &proBrightness });
        {
            auto a = proPanels[2];
            a.removeFromTop (32);
            a.reduce (6, 4);
            auto knobCell = a.removeFromLeft (a.getWidth() / 2).reduced (3, 0);
            magicKnob.label.setBounds (knobCell.removeFromTop (16));
            magicKnob.slider.setBounds (knobCell);
            autoLevelButton.setBounds (a.removeFromTop (a.getHeight() / 2).withSizeKeepingCentre (a.getWidth(), 34));
            safetyButton.setBounds (a.withSizeKeepingCentre (a.getWidth(), 34));
        }
        // row 2: Espacio | Mezcla
        const int w2 = rowMid.getWidth();
        proPanels[3] = rowMid.removeFromLeft (w2 * 42 / 100);
        rowMid.removeFromLeft (8);
        proPanels[4] = rowMid;
        {
            auto inner = proPanels[3];
            inner.removeFromTop (32);
            inner.reduce (6, 4);
            spatialButton.setBounds (inner.removeFromBottom (28));
            const int w = inner.getWidth() / 3;
            for (auto* k : { &proWidth, &proReverb, &proDelay })
            {
                auto cell = inner.removeFromLeft (w).reduced (3, 0);
                k->label.setBounds (cell.removeFromTop (16));
                k->slider.setBounds (cell);
            }
        }
        place (proPanels[4], { &proMix, &proPan, &proOffset, &proGain });
        // row 3: Calidad y diagnóstico
        proPanels[5] = rowBottom;
        {
            auto a = proPanels[5];
            a.removeFromTop (32);
            a.reduce (14, 2);
            auto toggles = a.removeFromTop (28);
            improvedButton.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2));
            engineButton.setBounds (toggles);
            engineInfo.setBounds (a);
        }
    }

    faderStrip = faderArea;
    if (second)
    {
        auto f = faderArea.reduced (10, 10);
        faderLabel.setBounds (f.removeFromTop (22));
        auto m = f.removeFromRight (16);
        secondMeter.setBounds (m.withTrimmedTop (4).withTrimmedBottom (26));
        fader.setBounds (f.withTrimmedTop (4));
    }
}

} // namespace vs
