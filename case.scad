$fn=360;
width=180;
height=100;
depth=25;
lcd_width=80;
lcd_height=52;
lcd_depth=9;
btn_width=12;



module cover() {
    difference() {
        union() {
            difference() {
                translate([0,0,-depth-lcd_depth])cube([width, height, depth+lcd_depth]);
                translate([3,3,-depth-lcd_depth])cube([width-6, height-6, depth+lcd_depth]);        
            }
            cube([width, height, 3]);
            translate([13+lcd_width/2, 31+lcd_height/2, -lcd_depth+3]) linear_extrude(height=lcd_depth, scale=1.3) square([lcd_width, lcd_height], center=true); // LCD
            translate([106-1, 24-1, -lcd_depth])color("blue") cube([btn_width+2, btn_width+2, lcd_depth]); // Button
            // fix holes
            translate([3,3,-depth-lcd_depth])cube([5,5,depth+lcd_depth]);
            translate([width-3-5,3,-depth-lcd_depth])cube([5,5,depth+lcd_depth]);
            translate([3,height-3-5,-depth-lcd_depth])cube([5,5,depth+lcd_depth]);
            translate([width-3-5,height-3-5,-depth-lcd_depth])cube([5,5,depth+lcd_depth]); 
        }
        //translate([13, 31, -1])color("blue") cube([lcd_width, lcd_height, 6]); // LCD position helper
        translate([13+lcd_width/2, 31+lcd_height/2, -lcd_depth+3])color("blue")linear_extrude(height=lcd_depth, scale=1.3) square([lcd_width-3, lcd_height-3], center=true); // LCD
        translate([106, 24, -lcd_depth-3])color("blue") cube([btn_width, btn_width, 20]); // Button
        
        translate([175, 43, -3-lcd_depth-9])color("blue") cube([6, 12, 6]); // USB
        translate([110, 80, 2.5]) linear_extrude(height = 2) color("green") text(text = "Pip-Boy 3000 Mk V", font = "Helvetica Neue:style=Condensed Bold", size = 6);
        translate([10, 10, 2.5]) linear_extrude(height = 2) color("green") text(text = "Vault-Tec Approved Air™ — Now with 12% less mystery!", font = "Helvetica Neue:style=Condensed Bold", size = 5);
        translate([140, 55, 4]) trefoil(13,3,1.5,5);
        // fix holes
        translate([4,4,-depth-lcd_depth-10]) cylinder(r=1.9, h=40);
        translate([width-4,4,-depth-lcd_depth-10]) cylinder(r=1.9, h=40);
        translate([4,height-4,-depth-lcd_depth-10]) cylinder(r=1.9, h=40);
        translate([width-4,height-4,-depth-lcd_depth-10]) cylinder(r=1.9, h=40);
    }
}

module trefoil(outradius,height,inRatio,outRatio){
	inradius = outradius/outRatio;
	union(){
		cylinder(r=inradius,height,center=true);
		difference(){
			cylinder(r=outradius,height,center=true);
			cylinder(r=inradius*inRatio,height*2,center=true);
			for(i=[0:3]){
				rotate(a = i*120) {
				linear_extrude(height=height+1, center=true)
				polygon(points=[[0,0],[2*outradius,0],[(2*outradius)*cos(60),(2*outradius)*sin(60)]], paths=[[0,1,2]]);
				}
			}
		}
	}
    difference() {
        cylinder(r=outradius, h= height, center=true);
        cylinder(r=outradius-1, h= height, center=true);
    }
    
}

module back() {
    difference() {
        union() {
            translate([0,0,0])cube([width, height, 3]);
            translate([width/2, height/2, 3]) cube([6, 8 , 10]);
        }
        translate([4,4,-1]) cylinder(r=1.9, h=40);
        translate([width-4,4,-1]) cylinder(r=1.9, h=40);
        translate([4,height-4,-1]) cylinder(r=1.9, h=40);
        translate([width-4,height-4,-1]) cylinder(r=1.9, h=40);
        translate([width/2-5, height/2+4, 8])    rotate([0,90,0]) cylinder(r=1.6,h=20);
        translate([width-20,height-20,-1]) cylinder(r=2.5, h=40);
    }    
}

module button() {
    // adjust the button depth
    btn_depth = 30;
    translate([0,0,btn_depth/2])cube([btn_width, btn_width, btn_depth], center=true);
    cube([btn_width+3, btn_width+3, 2], center=true);
    translate([-3, -3, btn_depth]) linear_extrude(height = 0.6) color("green") text(text = ">", font = "Helvetica Neue:style=Condensed Bold", size = 8);
}

module leg() {
    difference() {
        union() {
            cube([6+3*2, 8 , height/2-3]);
            translate([0, 4, height/2-3]) rotate([0, 90, 0]) cylinder(r=4, h=6+3*2);
        }
        translate([-1, 4, height/2-3]) rotate([0, 90, 0]) cylinder(r=1.5, h=20);
        translate([3, 4, height/2-3]) rotate([0, 90, 0]) cylinder(r=6, h=6);
    }  
    translate([-50/2+12/2, 4, 0]) rotate([0, 90, 0]) cylinder(r=4, h=50);
}

button();
//cover();
//back();
//leg();
 
