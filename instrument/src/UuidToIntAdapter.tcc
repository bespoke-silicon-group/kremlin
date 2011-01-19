template <typename Integer>
Integer UuidToIntAdapter<Integer>::get(const boost::uuids::uuid& id)
{
	Integer ret = 0;

	// return the XOR of the bits.
	for(size_t i = 0; i < sizeof(boost::uuids::uuid) / sizeof(Integer); i++)
		ret ^= ((const Integer*)id.data)[i];

    return ret;
}
