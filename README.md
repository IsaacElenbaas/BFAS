# Bezier Flat Art Studio
## About
BFAS is a program for creating art of a "flat" style - i.e. lineless art, even though support for lines is planned.

Its unique features are allowing artists to create shapes from one or more connected bezier curves and then color those shapes by specifying color points. The color for any pixel is determined using non-sibsonian interpolation of the provided color points for a shape.  
It does this all in a vector-ish way, allowing around a 1,000,000x zoom before problems arise.
## Usage
Be sure to set and apply your output aspect ratio prior to working - changing it later will stretch the canvas right now

Keybind customization is coming soonTM
```
Left click and drag to move a point without unmerging it from or merging it with other points
Right click and drag to move a point and unmerge it from or merge it with other points
Press A to add a bezier - this can be repeated to make a chain quickly, left click to end
Press S to select a shape under the cursor - press repeatedly to cycle through when there are multiple
Press D to deselect the current shape
Press F to view the final product - hides beziers, but doesn't have anti-aliasing like the actual output will
Press G to remove a point (and possibly beziers attached to it)
Press C to add a color at the mouse cursor to the selected shape - note that this can be outside of the shape
Press E to move the selected shape upwards and above other shapes
Press R to move the selected shape downwards and below other shapes
```
To edit a color, select its shape and click on the color point (unless you just added it). The color picker will then affect that color. You cannot press `D` while tweaking it, but you can press `F`.
## Current State Image
<img src="https://media.discordapp.net/attachments/378322175226150912/1144835182927548499/scrot_2023-08-25-12346_1335x1043.png"></img>
## Development Videos
### Quick Layer/Opacity Showcase:  


https://github.com/IsaacElenbaas/BFAS/assets/34344969/8e1ce98b-1b2e-4a80-8463-2df30d8f5941



### Zoom Capabilities:  


https://github.com/IsaacElenbaas/BFAS/assets/34344969/2e8b332a-b846-45fa-b442-6a581f790e37


## Issues You Should Know About
* Horizontal lines sometimes come from where beziers meet. This is high priority, but though it has a simple cause, it is not very approachable. I have tried many workarounds and nothing I have come up with has really had a chance of solving the issue. It is less common than it used to be, but there is not much to do about it other than try changing resolution or shift things a bit if the issue shows up in exported images.
* Loading saved projects does not clear the canvas. Close and reopen the program before loading anything.
* Resources do not get cleaned up yet. This should be unnoticeable unless you make something huge and delete it all, as they *do* get reused when available.
* I have not actually made anything substantial with this yet, only done "throw some shapes on there, connect a ton of stuff, add colors" testing. Things may break.
## Plans (in no order)
* All of the tools you would expect in a drawing or drafting application - color picking, color code input, trimming beziers with other beziers, splitting beziers where they intersect, cutting/copying and pasting, selection and transformation, etc.
* Miscellaneous tools such as canvas expansion or cropping, cutting or copying colors between shapes, deleting all colors from a shape, etc.
* Line support, including changing colors and thickness along lines
* Textures/shaders to overlay on shapes
* Other things in my notes on my other, unaccessible, machine which I cannot remember right now :)
