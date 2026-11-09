// =====================================================================
//  Gehaeuse fuer die Wetterstation  -  LILYGO T-Display-S3 (ohne Touch)
//  Stil: clean & modern, quer, auf Standfuss, nur USB-C (kein Akku)
//
//  Teile (alle ohne Stuetzstruktur druckbar, FDM 0,4 mm, PLA):
//    front    - Schale mit Displayfenster, Tasten und USB-C-Oeffnung
//               (Front nach unten aufs Druckbett)
//    deckel   - Rueckdeckel mit Schnapplaschen und Lueftungsschlitzen
//               (Aussenseite nach unten)
//    tasten   - 2 Tastenstoessel fuer IO0 (BOOT) und IO14
//               (Kappe nach unten)
//    staender - Standfuss, Neigung 15 Grad (Unterseite nach unten)
//
//  Ausgabe waehlen:  openscad -D 'teil="front"' -o front.stl wetterstation_gehaeuse.scad
//  teil = "alles" | "explosion" | "druckplatte" | "front" | "deckel" | "tasten" | "staender" | "platine"
//
//  Board-Masse aus LILYGOs eigenem 3D-Modell (Repo Xinyuan-LilyGO/T-Display-S3,
//  dimensions/t-display-s3-full.stp/.stl), siehe README in diesem Ordner.
// =====================================================================

teil = "alles";

/* [Druck / Toleranzen] */
spiel        = 0.25;   // Luft rund um Platine und Glas
spiel_deckel = 0.15;   // Luft zwischen Deckellaschen und Schalenwand
spiel_taste  = 0.15;   // Luft rund um die Tastenstoessel (radial)
spiel_fuss   = 0.25;   // Luft im Schlitz des Standfusses (je Seite)

/* [Gehaeuse] */
wand_front   = 1.6;    // Dicke der Frontplatte
deckel_dicke = 2.0;
fase         = 1.0;    // 45-Grad-Fase an Vorder- und Hinterkante
eck_radius   = 5;      // Radius der Ecken (Ansicht von vorn)
luft_hinten  = 1.0;    // Abstand hoechstes Bauteil -> Deckel
rueckraum    = 0;      // zusaetzlicher Platz hinten (z. B. 6 fuer eingeloetete Stiftleisten)
rand_tasten  = 3.2;    // Wandstaerke an der USB-Seite (aussen bis Platinenende -0,5)
rand_gegen   = 2.7;    // Wand auf der Gegenseite (ab Hohlraumende)
hoehe        = 35;     // Gesamthoehe (Ansicht von vorn)
reset_loch   = true;   // Loch fuer RST (Bueroklammer) unten in der Wand
lueftung     = true;

/* [Standfuss] */
fuss_neigung = 15;     // Grad nach hinten
fuss_laenge  = 52;
fuss_tiefe   = 38;
fuss_hoehe   = 15;     // hinten
fuss_vorn    = 8.5;    // vorn (Keilform, verdeckt das Display nicht)
fuss_boden   = 5;      // Hoehe des Schlitzbodens (Mitte)

$fn = 48;
e = 0.01;

// ---------------------------------------------------------------------
//  Platine (Masse LILYGO, Koordinaten im Gehaeuse):
//  X = entlang der Platine (0 = USB-Ende), Y = quer (oben +), Z = Tiefe (0 = Front aussen)
//  Platinen-Koordinaten (x quer, y laengs, z: Display -6,38 ... Bauteile +3,5)
//  -> Gehaeuse: X = y, Y = -x, Z = wand_front + 6,38 + z
// ---------------------------------------------------------------------
pcb_l   = 60.78;  pcb_b = 25.51;  pcb_d = 1.2;
glas_x0 = 6.66;   glas_x1 = 56.53;  glas_b = 25.95;  glas_d = 1.53;
akt_x0  = 12.18;  akt_x1  = 54.90;  akt_b  = 22.70;   // aktive Displayflaeche
abst_glas_pcb = 5.18;      // Glasvorderseite -> Platinenunterseite
bauteile_h    = 3.5;       // hoechstes Bauteil auf der Rueckseite (JST-Stecker)
usb_y0 = -4.34;  usb_y1 = 4.61;   usb_x0 = -1.2;   // USB-C (Displayseite)
usb_z0 = 2.02;   usb_z1 = 5.18;   // ab Glasvorderseite
taste_boot = [2.31,  9.29];       // IO0
taste_io14 = [2.20, -8.81];       // IO14
taste_hoehe = 2.68;               // Tastenkappe: Abstand ab Glasvorderseite
rst_pos = [10.3, -13.34];  rst_z = 7.36;   // RST seitlich, ab Glasvorderseite
loecher = [[57.66, 10.095], [57.66, -9.92]];  loch_d = 1.9;

// abgeleitete Hoehen (Z)
z_glas = wand_front;
z_pcb  = z_glas + abst_glas_pcb;          // Platinenunterseite
z_pcb_oben = z_pcb + pcb_d;
z_innen_deckel = z_pcb_oben + bauteile_h + luft_hinten + rueckraum;
tiefe  = z_innen_deckel + deckel_dicke;

// Hohlraum
hr_x0 = -0.5;  hr_x1 = pcb_l + 0.5;
hr_y_unten = glas_b/2 + spiel;     // bis Glas-/Tastenhoehe eng (fuehrt das Glas)
hr_y_oben  = 14.0;                 // darueber weiter (Platz fuer die Schnapplaschen)
z_stufe    = z_glas + 4;

// Aussenmasse
aus_x0 = hr_x0 - rand_tasten;
aus_x1 = hr_x1 + rand_gegen;
aus_l  = aus_x1 - aus_x0;
aus_mx = (aus_x0 + aus_x1)/2;

// Schnapplaschen
lasche_x   = [18, 44];
lasche_b   = 8;
lasche_l   = z_innen_deckel - (z_pcb_oben + 0.2);
lasche_d   = 1.0;
nase       = 0.45;
z_nase     = z_innen_deckel - lasche_l + 1.0;

// Tasten
kappe_d = 3.6;   loch_taste = kappe_d + 2*spiel_taste;
z_senk0 = 1.0;   senk_h = 0.8;     // 45-Grad-Sitz innen in der Frontplatte

echo(str("Aussenmasse: ", aus_l, " x ", hoehe, " x ", tiefe, " mm"));

// =====================================================================
module rund_quader(x0, x1, y0, y1, z0, z1, r, f) {
    // abgerundeter Quader mit 45-Grad-Fasen vorn und hinten
    hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y, 0]) {
        translate([0, 0, z0])      cylinder(r1 = r - f, r2 = r, h = f);
        translate([0, 0, z0 + f])  cylinder(r = r, h = z1 - z0 - 2*f);
        translate([0, 0, z1 - f])  cylinder(r1 = r, r2 = r - f, h = f);
    }
}

module koerper() rund_quader(aus_x0, aus_x1, -hoehe/2, hoehe/2, 0, tiefe, eck_radius, fase);

module langloch(b, h, l) { // Stadion in YZ-Ebene, Laenge l entlang X
    hull() for (s = [-1, 1]) translate([0, s*(b - h)/2, 0]) rotate([0, 90, 0]) cylinder(d = h, h = l);
}

// ---------------------------------------------------------------------
module front() {
    difference() {
        intersection() {
            koerper();
            translate([aus_x0 - 1, -hoehe, -1]) cube([aus_l + 2, 2*hoehe, z_innen_deckel + 1]);
        }
        // Hohlraum: unten eng (fuehrt Glas), oben weiter
        translate([hr_x0, -hr_y_unten, z_glas]) cube([hr_x1 - hr_x0, 2*hr_y_unten, 30]);
        translate([hr_x0, -hr_y_oben, z_stufe]) cube([hr_x1 - hr_x0, 2*hr_y_oben, 30]);

        // Displayfenster mit 45-Grad-Fase vorn
        fx0 = akt_x0 - 0.3; fx1 = akt_x1 + 0.3; fb = akt_b/2 + 0.3; fv = 1.0;
        hull() {
            translate([fx0, -fb, fv]) cube([fx1 - fx0, 2*fb, wand_front + 1]);
            translate([fx0 - fv, -fb - fv, -1]) cube([fx1 - fx0 + 2*fv, 2*fb + 2*fv, 1]);
        }

        // Tastenloecher mit 45-Grad-Sitz innen
        for (t = [taste_boot, taste_io14]) translate([t[0], t[1], 0]) {
            translate([0, 0, -1]) cylinder(d = loch_taste, h = wand_front + 2);
            translate([0, 0, z_senk0]) cylinder(d1 = loch_taste, d2 = loch_taste + 2*senk_h, h = senk_h + e);
        }

        // USB-C: Oeffnung fuer den Steckerkoerper
        translate([aus_x0 - 1, (usb_y0 + usb_y1)/2, z_glas + (usb_z0 + usb_z1)/2])
            langloch(12.8, 7.0, hr_x0 - aus_x0 + 2);

        // Nuten fuer die Schnapplaschen (45-Grad-Flanken, ohne Stuetzen)
        for (x = lasche_x, s = [-1, 1])
            translate([x, s*hr_y_oben, z_nase]) rotate([0, 90, 0]) rotate([0, 0, 45])
                cube([sqrt(2)*(nase + 0.15), sqrt(2)*(nase + 0.15), lasche_b + 2], center = true);

        // Hebelkerbe unten in der Trennfuge (Muenze/Fingernagel)
        translate([aus_mx - 5, -hoehe/2 - 1, z_innen_deckel - 0.7]) cube([10, 2.2, 1]);

        // Reset-Loch (unten, im Standfuss verborgen)
        if (reset_loch) translate([rst_pos[0], -hoehe/2 - 1, z_glas + rst_z]) rotate([-90, 0, 0]) cylinder(d = 1.6, h = 10, $fn = 24);
    }
    // Glasanschlaege quer (fuehren das Display in X)
    translate([glas_x0 - spiel - 1.0, -9, z_glas - e]) cube([1.0, 18, 1.4]);
    translate([glas_x1 + spiel, -7, z_glas - e]) cube([0.8, 14, 1.4]);
    // Auflage unter der USB-Buchse (traegt beim Einstecken)
    translate([hr_x0, usb_y0 - 0.3, z_glas - e]) cube([glas_x0 - spiel - 1.0 - hr_x0, usb_y1 - usb_y0 + 0.6, usb_z0 - 0.05 + e]);
    // Stuetzen mit Zapfen fuer die zwei Befestigungsloecher (D-foermig, Glas frei)
    for (l = loecher) {
        intersection() {
            translate([l[0], l[1], z_glas - e]) cylinder(d = 3.6, h = abst_glas_pcb + e);
            translate([glas_x1 + spiel + 0.05, -20, 0]) cube([10, 40, 20]);
        }
        translate([l[0], l[1], z_pcb - e]) cylinder(d = loch_d - 0.4, h = 1.0, $fn = 20);
    }
}

// ---------------------------------------------------------------------
module deckel() {
    difference() {
        union() {
            intersection() {
                koerper();
                translate([aus_x0 - 1, -hoehe, z_innen_deckel]) cube([aus_l + 2, 2*hoehe, 20]);
            }
            // Schnapplaschen mit Nase
            for (x = lasche_x, s = [-1, 1]) translate([x, 0, 0]) mirror([0, s < 0 ? 1 : 0, 0]) {
                yo = hr_y_oben - spiel_deckel;
                difference() {
                    translate([-lasche_b/2, yo - lasche_d, z_innen_deckel - lasche_l]) cube([lasche_b, lasche_d, lasche_l + e]);
                    // Einfuehrschraege an der Spitze
                    translate([0, yo, z_innen_deckel - lasche_l]) rotate([45, 0, 0]) cube([lasche_b + 1, 0.6, 0.6], center = true);
                }
                translate([0, yo, z_nase]) rotate([0, 90, 0]) rotate([0, 0, 45])
                    cube([sqrt(2)*nase, sqrt(2)*nase, lasche_b], center = true);
            }
            // Niederhalter: Rohre ueber den Zapfen, Rippen am USB-Ende
            for (l = loecher) translate([l[0], l[1], z_pcb_oben + 0.05])
                difference() { cylinder(d = 4.0, h = z_innen_deckel - z_pcb_oben); translate([0, 0, -1]) cylinder(d = 2.2, h = 3); }
            for (s = [-1, 1]) translate([1.0, s > 0 ? 11.95 : -12.7, z_pcb_oben + 0.05]) cube([4, 0.75, z_innen_deckel - z_pcb_oben]);
        }
        // Lueftungsschlitze
        if (lueftung) for (y = [-8, -4, 0, 4, 8]) hull() for (x = [15, 50])
            translate([x, y, z_innen_deckel - 1]) cylinder(d = 1.6, h = 10, $fn = 16);
        // Hebelkerbe
        translate([aus_mx - 5, -hoehe/2 - 1, z_innen_deckel - 0.3]) cube([10, 2.2, 1]);
    }
}

// ---------------------------------------------------------------------
module taste() {
    // Kappe steht 0,4 mm vor (z < 0); Sitz-Kegel liegt im 45-Grad-Sitz der Frontplatte
    vor = 0.4;
    kd  = kappe_d;
    z_k = z_senk0;                       // Beginn Kegel
    z_f = z_k + senk_h;                  // Kegel fertig
    z_ende = z_glas + taste_hoehe + 0.05; // sitzt der Kegel an, bleiben 0,1 mm Luft zur Tastenkappe
    translate([0, 0, -vor]) {
        hull() {                                           // leicht gewoelbte Kappe
            translate([0, 0, 0.3]) cylinder(d = kd, h = z_k + vor - 0.3);
            cylinder(d = kd - 0.6, h = e);
        }
        translate([0, 0, z_k + vor]) cylinder(d1 = kd, d2 = kd + 2*senk_h - 0.1, h = senk_h - 0.05);
        translate([0, 0, z_f + vor - 0.05]) cylinder(d = kd + 2*senk_h - 0.1, h = 0.45);
        translate([0, 0, z_f + vor + 0.4 - e]) cylinder(d = 2.4, h = z_ende - z_f - 0.4 + e);
    }
}

// ---------------------------------------------------------------------
//  Standfuss: eigenes Koordinatensystem, Z nach oben, Front zeigt nach -Y
schlitz_b = tiefe + 2*spiel_fuss;
fuss_y0   = -3;                        // Lage des Schlitzes (Mitte Schlitzboden)
module fuss_rahmen() {
    // Rahmen, in dem das Gehaeuse steckt (Unterkante Mitte = Ursprung, Gehaeuse-Dicke entlang +Y)
    translate([0, fuss_y0, fuss_boden]) rotate([-fuss_neigung, 0, 0]) children();
}
module staender() {
    r = 6; f = 2;
    difference() {
        hull() for (x = [-fuss_laenge/2 + r, fuss_laenge/2 - r], y = [-fuss_tiefe/2 + r, fuss_tiefe/2 - r]) translate([x, y, 0]) {
            h = y < 0 ? fuss_vorn : fuss_hoehe;
            cylinder(r = r, h = h - f);
            translate([0, 0, h - f]) cylinder(r1 = r, r2 = r - f, h = f);
        }
        fuss_rahmen() translate([-fuss_laenge, -schlitz_b/2, 0]) cube([2*fuss_laenge, schlitz_b, 40]);
    }
}

// Gehaeuse in den Fuss setzen: Gehaeuse-X -> -X, Gehaeuse-Y -> nach oben, Gehaeuse-Z -> nach hinten
module im_fuss() {
    fuss_rahmen() multmatrix([[-1, 0, 0, 0], [0, 0, 1, 0], [0, 1, 0, 0], [0, 0, 0, 1]])
        translate([-aus_mx, hoehe/2, -tiefe/2]) children();
}

// ---------------------------------------------------------------------
//  vereinfachte Platine fuer die Vorschau
module platine() {
    color("#1a1a1a") translate([0, -pcb_b/2, z_pcb]) cube([pcb_l, pcb_b, pcb_d]);
    color("#0b0b10") translate([glas_x0, -glas_b/2, z_glas]) cube([glas_x1 - glas_x0, glas_b, glas_d]);
    color("#3d6fd0") translate([akt_x0, -akt_b/2, z_glas - 0.02]) cube([akt_x1 - akt_x0, akt_b, 0.05]);
    color("silver") translate([usb_x0, usb_y0, z_glas + usb_z0]) cube([8.4, usb_y1 - usb_y0, usb_z1 - usb_z0]);
    for (t = [taste_boot, taste_io14]) {
        color("silver") translate([t[0] - 1.7, t[1] - 2.1, z_glas + taste_hoehe + 1]) cube([3.35, 4.2, 1.5]);
        color("white")  translate([t[0] - 1.0, t[1] - 1.33, z_glas + taste_hoehe]) cube([2.02, 2.67, 1.0]);
    }
    color("#e8e0c8") translate([0.3, -11.6, z_pcb_oben]) cube([4.7, 6.3, 2.5]);
    color("#e8e0c8") translate([1.1, -3.5, z_pcb_oben]) cube([4.4, 7.0, bauteile_h]);
    color("#555") translate([20, -9, z_pcb_oben]) cube([18, 18, 0.9]);
}

// ---------------------------------------------------------------------
farbe_schale = "#ececee";
farbe_deckel = "#d6d7da";
farbe_fuss   = "#c9a77c";
farbe_taste  = "#c9a77c";

module zusammen(explo = 0) {
    color(farbe_schale) front();
    translate([0, 0, explo]) color(farbe_deckel) deckel();
    for (t = [taste_boot, taste_io14]) translate([t[0], t[1], -explo*0.6]) color(farbe_taste) taste();
    translate([0, 0, explo*0.45]) platine();
}

if (teil == "alles") {
    im_fuss() zusammen();
    color(farbe_fuss) staender();
} else if (teil == "explosion") {
    zusammen(22);
} else if (teil == "front") {
    front();
} else if (teil == "deckel") {
    // Aussenseite nach unten
    translate([0, 0, tiefe]) rotate([180, 0, 0]) deckel();
} else if (teil == "deckel_eingebaut") {
    deckel();
} else if (teil == "taste") {
    taste();
} else if (teil == "tasten") {
    for (i = [0, 1]) translate([i*8, 0, 0.4]) taste();
} else if (teil == "staender") {
    staender();
} else if (teil == "druckplatte") {
    // alle Teile in Druckrichtung nebeneinander (nur zur Ansicht)
    color(farbe_schale) front();
    translate([0, hoehe + 8, 0]) color(farbe_deckel) translate([0, 0, tiefe]) rotate([180, 0, 0]) deckel();
    translate([aus_x1 + 10 + fuss_laenge/2, 8, 0]) color(farbe_fuss) staender();
    for (i = [0, 1]) translate([aus_x1 + 14 + i*8, hoehe + 8, 0.4]) color(farbe_taste) taste();
} else if (teil == "platine") {
    platine();
}
