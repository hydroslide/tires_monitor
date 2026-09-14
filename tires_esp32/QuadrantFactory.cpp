#include "QuadrantFactory.h"
#include "ThermalDisplay.h"
#include <Arduino.h>

QuadrantFactory::QuadrantFactory(DisplayBase &displayTFT, int margin)
  : tft(displayTFT), margin(margin)
{
    // Nothing else to do here
}

QuadrantFactory::~QuadrantFactory() {
    // No owned ThermalDisplay instances to free here. Caller is responsible.
}

ThermalDisplay* QuadrantFactory::createDisplay(bool top, bool left) {
    // 1) Determine the quadrant’s origin on the full 280×240 screen
    int quadX = left  ? 0               : QUAD_WIDTH;  // left half: x=0; right half: x=140
    int quadY = top   ? 0               : QUAD_HEIGHT; // top half: y=0; bottom half: y=120

    // 2) Compute the “usable” area inside that quadrant after subtracting margins
    //    We leave 'margin' pixels on all four sides, so:
    int contentW = QUAD_WIDTH  - 2 * margin; // width available for the scaled 32×24 image
    int contentH = QUAD_HEIGHT - 2 * margin; // height available

    // 3) Determine the integer scale factor that preserves 32:24 aspect ratio
    //    scaleX = contentW / 32, scaleY = contentH / 24 → take the smaller (floor)
    int scaleX = contentW / CAMERA_WIDTH;   // how many pixels wide per camera pixel
    int scaleY = contentH / CAMERA_HEIGHT;  // how many pixels tall per camera pixel
    int scale  = min(scaleX, scaleY);
    if (scale < 1) scale = 1; // ensure at least a 1:1 mapping even if margin is large

    // 4) Compute the final display‐region size for the camera image
    //    areaW = 32 × scale,  areaH = 24 × scale
    int areaW = CAMERA_WIDTH  * scale;
    int areaH = CAMERA_HEIGHT * scale;

    // 5) Center that area inside the “usable” content rectangle (contentW × contentH)
    //    offsetX′ = (contentW − areaW) / 2, offsetY′ = (contentH − areaH) / 2
    int offsetInsideX = (contentW - areaW) / 2;
    int offsetInsideY = (contentH - areaH) / 2;

    // 6) Convert the “inside‐quadrant” offsets back into screen coordinates:
    //    areaX = quadX + margin + offsetInsideX
    //    areaY = quadY + margin + offsetInsideY
    int areaX = quadX + margin + offsetInsideX;
    int areaY = quadY + margin + offsetInsideY;

    // Manual offset
    int yOffsetPixels = 3;
    areaY+= (yOffsetPixels * ((top)? 1:-1));
    int xOffsetPixels = 2;
    areaX+= (xOffsetPixels * ((left)? 1:-1));

    // 7) Instantiate a ThermalDisplay for that rectangle
    //    Constructor: ThermalDisplay(tft, areaX, areaY, areaW, areaH)
    ThermalDisplay* disp = new ThermalDisplay(tft, areaX, areaY, areaW, areaH);
    return disp;
}
