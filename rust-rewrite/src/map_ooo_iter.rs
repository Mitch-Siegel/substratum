#[allow(unused_imports)]
pub use btree_map_ooo_iter::*;
#[allow(unused_imports)]
pub use hash_map_ooo_iter::*;

/*
 * TODO: make this more library-like by genericizing errors?
 */

mod hash_map_ooo_iter {
    use std::collections::{HashMap, HashSet, VecDeque};
    use std::hash::Hash;
    fn check_hash_key_order<'a, K, V>(
        map: &'a HashMap<K, V>,
        mut key_order: impl Iterator<Item = K>,
    ) where
        K: Eq + Hash,
    {
        let mut seen_keys = HashSet::<K>::new();
        while let Some(key) = key_order.next() {
            assert!(
                map.contains_key(&key),
                "All keys in key ordering for out-of-order Map iterator must be present in map"
            );
            assert!(
                !seen_keys.contains(&key),
                "Duplicate key seen in key ordering for out-of-order Map iterator",
            );
            seen_keys.insert(key);
        }

        assert_eq!(seen_keys.len(), map.len(),
        "Missing key(s) from key ordering for out-of-order Map iterator. All keys must be included"
    );
    }

    // the iterator itself only needs to own Key, &Value pairs
    pub struct HashMapOOOIter<'a, K, V> {
        references: VecDeque<(K, &'a V)>,
    }

    // iteration is simply popping from the references vec
    impl<'a, K, V> Iterator for HashMapOOOIter<'a, K, V> {
        type Item = (K, &'a V);

        fn next(&mut self) -> Option<Self::Item> {
            self.references.pop_back()
        }
    }

    impl<'a, K, V> HashMapOOOIter<'a, K, V>
    where
        K: Eq + Hash,
    {
        pub fn new(map: &'a HashMap<K, V>, key_order: impl Iterator<Item = K> + Clone) -> Self {
            check_hash_key_order(map, key_order.clone());

            // allocate the vector with its full capacity from the get-go
            let mut references = VecDeque::<(K, &'a V)>::with_capacity(map.len());
            // grab values for each key, moving each key into the references vector along with its corresponding value reference
            for key in key_order {
                let value_ref: &V = map.get(&key).unwrap();
                references.push_front((key, value_ref));
            }

            HashMapOOOIter { references }
        }
    }

    pub struct HashMapOOOIterMut<'a, K, V> {
        references: VecDeque<(K, &'a mut V)>,
    }

    impl<'a, K, V> Iterator for HashMapOOOIterMut<'a, K, V> {
        type Item = (K, &'a mut V);

        fn next(&mut self) -> Option<Self::Item> {
            self.references.pop_back()
        }
    }

    impl<'a, K, V> HashMapOOOIterMut<'a, K, V>
    where
        K: Eq + Hash,
    {
        pub fn new(map: &'a mut HashMap<K, V>, key_order: impl Iterator<Item = K> + Clone) -> Self {
            check_hash_key_order(map, key_order.clone());

            // same as for HashMapOOOIter but with additional reference manipulation
            let mut references = VecDeque::<(K, &'a mut V)>::with_capacity(map.len());
            for key in key_order {
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
                references.push_front((key, value_mut));
            }

            HashMapOOOIterMut { references }
        }
    }

    #[cfg(test)]
    mod tests {
        use super::*;
        use std::collections::{HashMap, VecDeque};

        #[test]
        fn ooo_iter() {
            let mut map = HashMap::<usize, usize>::new();

            for i in 0..10 {
                map.insert(i, i * i);
            }

            let ordering = VecDeque::from(vec![2, 6, 4, 7, 8, 1, 3, 0, 5, 9]);
            let ooo_iter = HashMapOOOIter::new(&map, ordering.clone().into_iter());
            for (index, (key, value)) in ooo_iter.enumerate() {
                assert_eq!(key, ordering[index]);
                assert_eq!(key * key, *value);
            }

            let ooo_iter_mut = HashMapOOOIterMut::new(&mut map, ordering.clone().into_iter());
            for (index, (key, value)) in ooo_iter_mut.enumerate() {
                assert_eq!(key, ordering[index]);
                assert_eq!(key * key, *value);
            }
        }
    }
}

mod btree_map_ooo_iter {
    use std::collections::{BTreeMap, BTreeSet, VecDeque};
    fn check_btree_key_order<'a, K, V>(
        map: &'a BTreeMap<K, V>,
        mut key_order: impl Iterator<Item = K>,
    ) where
        K: Eq + Ord,
    {
        let mut seen_keys = BTreeSet::<K>::new();
        while let Some(key) = key_order.next() {
            assert!(
                map.contains_key(&key),
                "All keys in key ordering for out-of-order Map iterator must be present in map"
            );
            assert!(
                !seen_keys.contains(&key),
                "Duplicate key seen in key ordering for out-of-order Map iterator",
            );
            seen_keys.insert(key);
        }

        assert_eq!(seen_keys.len(), map.len(),
        "Missing key(s) from key ordering for out-of-order Map iterator. All keys must be included"
    );
    }

    // the iterator itself only needs to own Key, &Value pairs
    pub struct BTreeMapOOOIter<'a, K, V> {
        references: VecDeque<(K, &'a V)>,
    }

    // iteration is simply popping from the references vec
    impl<'a, K, V> Iterator for BTreeMapOOOIter<'a, K, V> {
        type Item = (K, &'a V);

        fn next(&mut self) -> Option<Self::Item> {
            self.references.pop_back()
        }
    }

    impl<'a, K, V> BTreeMapOOOIter<'a, K, V>
    where
        K: Eq + Ord,
    {
        pub fn new(map: &'a BTreeMap<K, V>, key_order: impl Iterator<Item = K> + Clone) -> Self {
            check_btree_key_order(map, key_order.clone());

            // allocate the vector with its full capacity from the get-go
            let mut references = VecDeque::<(K, &'a V)>::with_capacity(map.len());
            // grab values for each key, moving each key into the references vector along with its corresponding value reference
            for key in key_order {
                let value_ref: &V = map.get(&key).unwrap();
                references.push_front((key, value_ref));
            }

            BTreeMapOOOIter { references }
        }
    }

    pub struct BTreeMapOOOIterMut<'a, K, V> {
        references: VecDeque<(K, &'a mut V)>,
    }

    impl<'a, K, V> Iterator for BTreeMapOOOIterMut<'a, K, V> {
        type Item = (K, &'a mut V);

        fn next(&mut self) -> Option<Self::Item> {
            self.references.pop_back()
        }
    }

    impl<'a, K, V> BTreeMapOOOIterMut<'a, K, V>
    where
        K: Eq + Ord,
    {
        pub fn new(
            map: &'a mut BTreeMap<K, V>,
            key_order: impl Iterator<Item = K> + Clone,
        ) -> Self {
            check_btree_key_order(map, key_order.clone());

            // same as for HashMapOOOIter but with additional reference manipulation
            let mut references = VecDeque::<(K, &'a mut V)>::with_capacity(map.len());
            for key in key_order {
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
                references.push_front((key, value_mut));
            }

            BTreeMapOOOIterMut { references }
        }
    }

    #[cfg(test)]
    mod tests {
        use super::*;
        use std::collections::{BTreeMap, VecDeque};

        #[test]
        fn ooo_iter() {
            let mut map = BTreeMap::<usize, usize>::new();

            for i in 0..10 {
                map.insert(i, i * i);
            }

            let ordering = VecDeque::from(vec![2, 6, 4, 7, 8, 1, 3, 0, 5, 9]);
            let ooo_iter = BTreeMapOOOIter::new(&map, ordering.clone().into_iter());
            for (index, (key, value)) in ooo_iter.enumerate() {
                assert_eq!(key, ordering[index]);
                assert_eq!(key * key, *value);
            }

            let ooo_iter_mut = BTreeMapOOOIterMut::new(&mut map, ordering.clone().into_iter());
            for (index, (key, value)) in ooo_iter_mut.enumerate() {
                assert_eq!(key, ordering[index]);
                assert_eq!(key * key, *value);
            }
        }
    }
}
