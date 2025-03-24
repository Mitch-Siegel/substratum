use std::collections::{HashMap, HashSet};
use std::hash::Hash;

fn check_key_order<K, V>(map: &HashMap<K, V>, key_order: &Vec<K>)
where
    K: Eq + Hash,
{
    let mut seen_keys = HashSet::<&K>::new();
    for key in key_order {
        assert!(
            map.contains_key(key),
            "All keys in key ordering for out-of-order HashMap iterator must be present in map"
        );
        assert!(
            !seen_keys.contains(key),
            "Duplicate key seen in key ordering for out-of-order HashMap iterator",
        );
        seen_keys.insert(key);
    }

    assert!(
        seen_keys.len() == map.len(),
        "Missing key(s) from key ordering for out-of-order HashMap iterator. All keys must be included"
    );
}

// the iterator itself only needs to own Key, &Value pairs
pub struct HashMapOOOIter<'a, K, V> {
    references: Vec<(K, &'a V)>,
}

// iteration is simply popping from the references vec
impl<'a, K, V> Iterator for HashMapOOOIter<'a, K, V> {
    type Item = (K, &'a V);

    fn next(&mut self) -> Option<Self::Item> {
        self.references.pop()
    }
}

impl<'a, K, V> HashMapOOOIter<'a, K, V>
where
    K: Eq + Hash,
{
    pub fn new(map: &'a HashMap<K, V>, key_order: Vec<K>) -> Self {
        check_key_order(map, &key_order);

        // allocate the vector with its full capacity from the get-go
        let mut references = Vec::<(K, &'a V)>::with_capacity(map.len());
        // grab values for each key, moving each key into the references vector along with its corresponding value reference
        for key in key_order.into_iter().rev() {
            let value_ref: &V = map.get(&key).unwrap();
            references.push((key, value_ref));
        }

        HashMapOOOIter { references }
    }
}

pub struct HashMapOOOIterMut<'a, K, V> {
    references: Vec<(K, &'a mut V)>,
}

impl<'a, K, V> Iterator for HashMapOOOIterMut<'a, K, V> {
    type Item = (K, &'a mut V);

    fn next(&mut self) -> Option<Self::Item> {
        self.references.pop()
    }
}

impl<'a, K, V> HashMapOOOIterMut<'a, K, V>
where
    K: Eq + Hash,
{
    pub fn new(map: &'a mut HashMap<K, V>, key_order: Vec<K>) -> Self {
        check_key_order(map, &key_order);

        // same as for HashMapOOOIter but with additional reference manipulation
        let mut references = Vec::<(K, &'a mut V)>::with_capacity(map.len());
        for key in key_order.into_iter().rev() {
            // get our value reference as normal
            let value: &V = map.get(&key).unwrap();
            // create a pointer from the reference, and cast it to a mutable pointer
            let pointer: *const V = std::ptr::from_ref(value);
            let mut_pointer: *mut V = pointer as *mut V;
            // since we have exactly one instance of every key in the map per check_key_order()
            let value_mut: &mut V = unsafe {
                // trust that we can .as_mut() the pointer into a mutable reference
                mut_pointer.as_mut()
            }
            .unwrap();
            references.push((key, value_mut));
        }

        HashMapOOOIterMut { references }
    }
}
