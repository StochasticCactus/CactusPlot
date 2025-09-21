/* dataset definitions extracted from old_main.rs */

#[derive(Clone)]
#[derive(Debug)]
pub struct Dataset {
    pub name: String,
    pub points: Vec<[f64; 2]>,
    pub color: [u8; 3], // RGB color for this dataset
}

impl Dataset {
    pub fn new(name: String, points: Vec<[f64; 2]>, color: [u8; 3]) -> Self {
        Self {
            name,
            points,
            color,
        }
    }
    
    // Get display name for the dataset (used in legend and UI)
    pub fn display_name(&self) -> &str {
        &self.name
    }
    
    // Set a new display name
    pub fn set_name(&mut self, new_name: String) {
        self.name = new_name;
    }
}
