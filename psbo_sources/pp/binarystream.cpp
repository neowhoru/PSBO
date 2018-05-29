#include "stdafx.h"
#include "binarystream.h"

namespace
{
	void BinaryStreamDumper(std::ostream & o, const char * data, unsigned data_size)
	{
		o << "Length: " << data_size << '(' << std::hex << "0x" << data_size << ")\r\n";
		for (unsigned i = 0; i < data_size; ++i)
		{
			if ((i % 8) == 0 && i > 0)
			{
				if ((i % 16) == 0)
					o << "\r\n";
				else
					o << ' ';
			}

			o << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)(unsigned char)data[i] << ' ';
		}
		o << std::endl;
	}
};

std::ostream & operator << (std::ostream & o, BinaryReader const & br)
{
	BinaryStreamDumper(o, br.data, br.data_size);
	return o;
}


std::ostream & operator << (std::ostream & o, BinaryWriter & bw)
{
	BinaryStreamDumper(o, &bw.data[0], bw.data.size());
	return o;
}

unsigned BinaryReader::read_length()
{
	return read<unsigned>();
}


void BinaryWriter::write_length(unsigned len)
{
	write(len);
}

void BinaryReader::_read(BinaryWriter & bw)
{
	_read(bw.data);
	/*
	if (data_size - pos > 0)
	{
		bw.writeArray(&data[pos], data_size - pos);
		pos = data_size;
	}
	*/
}

