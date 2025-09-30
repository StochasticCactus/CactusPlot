/* dataset definitions extracted from old_main.rs */

#[derive(Clone)]
#[derive(Debug)]
/// Data structure used in dataset.rs module
pub struct Dataset {
    pub name: String,
    pub points: Vec<[f64; 2]>,
    pub color: [u8; 3], // RGB color for this dataset
}

/// Implementation block defining methods for this type
impl Dataset {
/// Function: explain its purpose and key arguments
    pub fn new(name: String, points: Vec<[f64; 2]>, color: [u8; 3]) -> Self {
        Self {
            name,
            points,
            color,
        }
    }
    
    // Get display name for the dataset (used in legend and UI)
/// Function: explain its purpose and key arguments
    pub fn display_name(&self) -> &str {
        &self.name
    }
    
    // Set a new display name
/// Function: explain its purpose and key arguments
    pub fn set_name(&mut self, new_name: String) {
        self.name = new_name;
    }
}