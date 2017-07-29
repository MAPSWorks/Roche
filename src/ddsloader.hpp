#pragma once

#include <string>
#include <vector>

/**
 * Loads DDS files from file system
 */
class DDSLoader
{
public:
	/**
	 * Block Compression Formats
	 */
	enum class Format
	{
		Undefined, 
		BC1, BC1_SRGB, 
		BC2, BC2_SRGB,
		BC3, BC3_SRGB,
		BC4, BC4_SIGNED,
		BC5, BC5_SIGNED,
		BC6, BC6_SIGNED,
		BC7, BC7_SRGB
	};

	DDSLoader() = default;
	/** 
	 * Opens a DDS file and extracts header data for subsequent reads
	 * @param filename DDS file path
	 */
	explicit DDSLoader(const std::string &filename);
	/**
	 * Returns the number of mipmaps in this file
	 */
	int getMipmapCount() const;
	/**
	 * Returns the width of a given mipmap level
	 */
	int getWidth(int mipmapLevel) const;
	/**
	 * Returns the height of a given mipmap level
	 */
	int getHeight(int mipmapLevel) const;
	/**
	 * Returns the block compression format
	 */
	Format getFormat() const;
	/**
	 * Returns the size in bytes of a mipmap level
	 * @param mipmapLevel mipmap level to read from
	 * @return size in bytes of the mipmap level
	 */
	size_t getImageSize(int mipmapLevel) const;
	/**
	 * Returns the image data of a mipmap level
	 * @param mipmapLevel mipmap level to read from
	 * @return iamge data as a byte vector
	 */
	std::vector<uint8_t> getImageData(int mipmapLevel) const;
	/**
	 * Write the image data of a mipmap level to a pointer
	 * @param mipmapLevel mipmap level to read from
	 * @param ptr to write to
	 */
	void writeImageData(int mipmapLevel, void* ptr) const;

private:
	/// Filename
	std::string _filename = "";
	/// Number of mipmap levels
	int _mipmapCount = 0;
	/// Width of largest mipmap level
	int _width = 0;
	/// Height of largest mipmap level
	int _height = 0;
	/// BC Format
	Format _format = Format::Undefined;
	/// Offsets in bytes of each mipmap level
	std::vector<int> _offsets;
	/// Size in bytes of each mipmap level
	std::vector<int> _sizes;
};