/* dataset definitions extracted from old_main.rs */

pub struct Dataset {
    pub name: String,
    pub points: Vec<[f64; 2]>,
    pub color: [u8; 3], // RGB color for this dataset
}
